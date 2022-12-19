#include <cassert>
#include <set>
#include <vector>
#include <HDTEnums.hpp>
#include <HDTManager.hpp>
#include <HDTVocabulary.hpp>
#include "OstrichStore.h"


/******** Construction and destruction ********/

Nan::Persistent<v8::Function> OstrichStore::constructor;

// Creates a new Ostrich store.
OstrichStore::OstrichStore(std::string path, const v8::Local<v8::Object> &handle, Controller *controller) : path(std::move(path)), controller(controller), features(1) {
    this->Wrap(handle);
}

// Deletes the Ostrich store.
OstrichStore::~OstrichStore() {
    Destroy(false);
}

// Destroys the document, disabling all further operations.
void OstrichStore::Destroy(bool remove) {
    if (controller != nullptr) {
        if (remove) {
            Controller::cleanup(path, controller);
        } else {
            delete controller;
        }
        controller = nullptr;
    }
}

// Constructs a JavaScript wrapper for an Ostrich store.
NAN_METHOD(OstrichStore::New) {
    assert(info.IsConstructCall());
    info.GetReturnValue().Set(info.This());
}

// Returns the constructor of OstrichStore.
const Nan::Persistent<v8::Function> &OstrichStore::GetConstructor() {
    if (constructor.IsEmpty()) {
        // Create constructor template
        v8::Local<v8::FunctionTemplate> constructorTemplate = Nan::New<v8::FunctionTemplate>(New);
        constructorTemplate->SetClassName(Nan::New("OstrichStore").ToLocalChecked());
        constructorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
        // Create prototype
        Nan::SetPrototypeMethod(constructorTemplate, "_searchTriplesVersionMaterialized", SearchTriplesVersionMaterialized);
        Nan::SetPrototypeMethod(constructorTemplate, "_searchTriplesDeltaMaterialized", SearchTriplesDeltaMaterialized);
        Nan::SetPrototypeMethod(constructorTemplate, "_searchTriplesVersion", SearchTriplesVersion);
        Nan::SetPrototypeMethod(constructorTemplate, "_append", Append);
        Nan::SetPrototypeMethod(constructorTemplate, "_close", Close);
        Nan::SetAccessor(constructorTemplate->PrototypeTemplate(), Nan::New("maxVersion").ToLocalChecked(), MaxVersion);
        Nan::SetAccessor(constructorTemplate->PrototypeTemplate(), Nan::New("_features").ToLocalChecked(), Features);
        Nan::SetAccessor(constructorTemplate->PrototypeTemplate(), Nan::New("closed").ToLocalChecked(), Closed);
        // Set constructor
        constructor.Reset(constructorTemplate->GetFunction(Nan::GetCurrentContext()).ToLocalChecked());
    }
    return constructor;
}


/******** createOstrichStore ********/

class CreateWorker : public Nan::AsyncWorker {
    std::string path;
    Controller *controller;
    bool read_only;
    SnapshotCreationStrategy *strategy;

public:
    CreateWorker(const char *path, bool read_only, const std::string& strategy_name, const std::string& strategy_parameter, Nan::Callback *callback)
            : Nan::AsyncWorker(callback), path(path), read_only(read_only), controller(nullptr),
              strategy(SnapshotCreationStrategy::get_composite_strategy(strategy_name, strategy_parameter)) {};

    void Execute() override {
        try {
            controller = new Controller(path, strategy, kyotocabinet::HashDB::TCOMPRESS, read_only);
        } catch (const std::invalid_argument &error) {
            SetErrorMessage(error.what());
        }
    }

    void HandleOKCallback() override {
        Nan::HandleScope scope;
        // Create a new OstrichStore
        v8::Local<v8::Object> newStore = Nan::NewInstance(Nan::New(OstrichStore::GetConstructor())).ToLocalChecked();
        new OstrichStore(path, newStore, controller);
        // Send the new OstrichStore through the callback
        const unsigned argc = 2;
        v8::Local<v8::Value> argv[argc] = {Nan::Null(), newStore};
        // callback->Call(argc, argv);
        Nan::Call(*callback, argc, argv);
    }
};

// Creates a new instance of OstrichStore.
// JavaScript signature: createOstrichStore(path, readOnly, callback)
NAN_METHOD(OstrichStore::Create) {
    assert(info.Length() == 4);
    Nan::AsyncQueueWorker(new CreateWorker(*Nan::Utf8String(info[0]),
                                           info[1]->BooleanValue(info.GetIsolate()),
                                           *Nan::Utf8String(info[2]),
                                           *Nan::Utf8String(info[3]),
                                           new Nan::Callback(info[4].As<v8::Function>())));
}


/******** OstrichStore#_searchTriplesVersionMaterialized ********/

class SearchTriplesVersionMaterializedWorker : public Nan::AsyncWorker {
    OstrichStore *store;
    // JavaScript function arguments
    std::string subject, predicate, object;
    uint32_t offset, limit;
    v8::Persistent<v8::Object> self;
    // Callback return values
    std::vector<Triple> triples;
    int version;
    uint32_t totalCount{0};
    bool hasExactCount{false};
    std::shared_ptr<DictionaryManager> dict;

public:
    SearchTriplesVersionMaterializedWorker(OstrichStore *store, char *subject, char *predicate, char *object,
                                           uint32_t offset, uint32_t limit, int32_t version, Nan::Callback *callback,
                                           v8::Local<v8::Object> self)
            : Nan::AsyncWorker(callback),
              store(store), subject(subject), predicate(predicate), object(object),
              offset(offset), limit(limit), version(version) {
        SaveToPersistent("self", self);
    };

    void Execute() override {
        TripleIterator *it = nullptr;
        try {
            Controller *controller = store->GetController();

            // Check version
            version = version >= 0 ? version : controller->get_max_patch_id();

            // Prepare the triple pattern
            dict = controller->get_dictionary_manager(version);
            Triple triple_pattern(subject, predicate, toHdtLiteral(object), dict);

            // Estimate the total number of triples
            std::pair<size_t, hdt::ResultEstimationType> count_data = controller->get_version_materialized_count(triple_pattern, version, true);
            totalCount = count_data.first;
            it = controller->get_version_materialized(triple_pattern, offset, version);
            hasExactCount = count_data.second == hdt::EXACT;

            // Add matching triples to the result vector
            Triple t;
            long count = 0;

            while (it->next(&t) && (limit == 0 || triples.size() < limit)) {
                triples.push_back(t);
                count++;
            }
        } catch (const std::runtime_error &error) {
            SetErrorMessage(error.what());
        }
        delete it;
    }

    void HandleOKCallback() override {
        Nan::HandleScope scope;

        // Convert the triples into a JavaScript object array
        uint32_t count = 0;
        v8::Local<v8::Array> triplesArray = Nan::New<v8::Array>(triples.size());
        const v8::Local<v8::String> SUBJECT = Nan::New("subject").ToLocalChecked();
        const v8::Local<v8::String> PREDICATE = Nan::New("predicate").ToLocalChecked();
        const v8::Local<v8::String> OBJECT = Nan::New("object").ToLocalChecked();
        for (auto& triple : triples) {
            v8::Local<v8::Object> tripleObject = Nan::New<v8::Object>();
            tripleObject->Set(Nan::GetCurrentContext(), SUBJECT, Nan::New(triple.get_subject(*dict).c_str()).ToLocalChecked());
            tripleObject->Set(Nan::GetCurrentContext(), PREDICATE, Nan::New(triple.get_predicate(*dict).c_str()).ToLocalChecked());
            std::string object = triple.get_object(*dict);
            tripleObject->Set(Nan::GetCurrentContext(), OBJECT, Nan::New(fromHdtLiteral(object).c_str()).ToLocalChecked());
            triplesArray->Set(Nan::GetCurrentContext(), count++, tripleObject);
        }

        // Send the JavaScript array and estimated total count through the callback
        const unsigned argc = 4;
        v8::Local<v8::Value> argv[argc] = {Nan::Null(), triplesArray, Nan::New<v8::Integer>((uint32_t) totalCount), Nan::New<v8::Boolean>((bool) hasExactCount)};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), argc, argv);
    }

    void HandleErrorCallback() override {
        Nan::HandleScope scope;
        v8::Local<v8::Value> argv[] = {v8::Exception::Error(Nan::New(ErrorMessage()).ToLocalChecked())};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), 1, argv);
    }
};

// Searches for a triple pattern in the document.
// JavaScript signature: OstrichStore#_searchTriplesVersionMaterialized(subject, predicate, object, offset, limit, version, callback)
NAN_METHOD(OstrichStore::SearchTriplesVersionMaterialized) {
    assert(info.Length() == 8);
    Nan::AsyncQueueWorker(new SearchTriplesVersionMaterializedWorker(Unwrap<OstrichStore>(info.This()),
                                                                     *Nan::Utf8String(info[0]),
                                                                     *Nan::Utf8String(info[1]),
                                                                     *Nan::Utf8String(info[2]),
                                                                     info[3]->Uint32Value(Nan::GetCurrentContext()).FromJust(),
                                                                     info[4]->Uint32Value(Nan::GetCurrentContext()).FromJust(),
                                                                     info[5]->Uint32Value(Nan::GetCurrentContext()).FromJust(),
                                                                     new Nan::Callback(info[6].As<v8::Function>()), info[7]->IsObject() ? info[7].As<v8::Object>()
                                                                             : info.This()));
}

/******** OstrichStore#_searchTriplesDeltaMaterialized ********/
class SearchTriplesDeltaMaterializedWorker : public Nan::AsyncWorker {
    OstrichStore *store;
    // JavaScript function arguments
    std::string subject, predicate, object;
    uint32_t offset, limit;
    v8::Persistent<v8::Object> self;
    // Callback return values
    std::vector<TripleDelta *> triples;
    int version_start, version_end;
    uint32_t totalCount{0};
    bool hasExactCount;
    std::shared_ptr<DictionaryManager> dict;

public:
    SearchTriplesDeltaMaterializedWorker(OstrichStore *store, char *subject, char *predicate, char *object,
                                         uint32_t offset, uint32_t limit, int32_t version_start, int32_t version_end,
                                         Nan::Callback *callback, v8::Local<v8::Object> self)
            : Nan::AsyncWorker(callback),
              store(store), subject(subject), predicate(predicate), object(object),
              offset(offset), limit(limit), version_start(version_start), version_end(version_end) {
        SaveToPersistent("self", self);
    };

    void Execute() {
        TripleDeltaIterator *it = nullptr;
        try {
            Controller *controller = store->GetController();

            // Check version
            version_end = version_end >= 0 ? version_end : controller->get_max_patch_id();

            // Prepare the triple pattern
            dict = controller->get_dictionary_manager(version_start);
            Triple triple_pattern(subject, predicate, toHdtLiteral(object), dict);

            // Estimate the total number of triples
            std::pair<size_t, hdt::ResultEstimationType> count_data = controller->get_delta_materialized_count(triple_pattern, version_start, version_end, true);
            totalCount = count_data.first;
            it = controller->get_delta_materialized(triple_pattern, offset, version_start, version_end);
            hasExactCount = count_data.second == hdt::EXACT;

            // Add matching triples to the result vector
            TripleDelta t;
            long count = 0;
            while (it->next(&t) && (!limit || triples.size() < limit)) {
                triples.push_back(new TripleDelta(new Triple(*t.get_triple()), t.is_addition()));
                count++;
            }
        } catch (const std::runtime_error &error) {
            SetErrorMessage(error.what());
        }
        delete it;
    }

    void HandleOKCallback() {
        Nan::HandleScope scope;

        // Convert the triples into a JavaScript object array
        uint32_t count = 0;
        v8::Local<v8::Array> triplesArray = Nan::New<v8::Array>(triples.size());
        const v8::Local<v8::String> SUBJECT = Nan::New("subject").ToLocalChecked();
        const v8::Local<v8::String> PREDICATE = Nan::New("predicate").ToLocalChecked();
        const v8::Local<v8::String> OBJECT = Nan::New("object").ToLocalChecked();
        const v8::Local<v8::String> ADDITION = Nan::New("addition").ToLocalChecked();
        for (auto& triple : triples) {
            v8::Local<v8::Object> tripleObject = Nan::New<v8::Object>();
            tripleObject->Set(Nan::GetCurrentContext(), SUBJECT, Nan::New(triple->get_triple()->get_subject(*dict).c_str()).ToLocalChecked());
            tripleObject->Set(Nan::GetCurrentContext(), PREDICATE, Nan::New(triple->get_triple()->get_predicate(*dict).c_str()).ToLocalChecked());
            std::string object = triple->get_triple()->get_object(*dict);
            tripleObject->Set(Nan::GetCurrentContext(), OBJECT, Nan::New(fromHdtLiteral(object).c_str()).ToLocalChecked());
            tripleObject->Set(Nan::GetCurrentContext(), ADDITION, Nan::New(triple->is_addition()));
            triplesArray->Set(Nan::GetCurrentContext(), count++, tripleObject);
            delete triple;
        }

        // Send the JavaScript array and estimated total count through the callback
        const unsigned argc = 4;
        v8::Local<v8::Value> argv[argc] = {Nan::Null(), triplesArray, Nan::New<v8::Integer>((uint32_t) totalCount), Nan::New<v8::Boolean>((bool) hasExactCount)};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), argc, argv);
    }

    void HandleErrorCallback() {
        Nan::HandleScope scope;
        v8::Local<v8::Value> argv[] = {v8::Exception::Error(Nan::New(ErrorMessage()).ToLocalChecked())};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), 1, argv);
    }
};

// Searches for a triple pattern in the document.
// JavaScript signature: OstrichStore#_searchTriplesDeltaMaterialized(subject, predicate, object, offset, limit, version, callback)
NAN_METHOD(OstrichStore::SearchTriplesDeltaMaterialized) {
    assert(info.Length() == 8);
    Nan::AsyncQueueWorker(new SearchTriplesDeltaMaterializedWorker(Unwrap<OstrichStore>(info.This()),
                                                                   *Nan::Utf8String(info[0]), *Nan::Utf8String(info[1]),
                                                                   *Nan::Utf8String(info[2]),
                                                                   info[3]->Uint32Value(Nan::GetCurrentContext()).FromJust(),
                                                                   info[4]->Uint32Value(Nan::GetCurrentContext()).FromJust(),
                                                                   info[5]->Uint32Value(Nan::GetCurrentContext()).FromJust(),
                                                                   info[6]->Uint32Value(Nan::GetCurrentContext()).FromJust(),
                                                                   new Nan::Callback(info[7].As<v8::Function>()),
                                                                   info[8]->IsObject() ? info[8].As<v8::Object>()
                                                                                       : info.This()));
}

/******** OstrichStore#_searchTriplesVersion ********/

class SearchTriplesVersionWorker : public Nan::AsyncWorker {
    OstrichStore *store;
    // JavaScript function arguments
    std::string subject, predicate, object;
    uint32_t offset, limit;
    v8::Persistent<v8::Object> self;
    // Callback return values
    std::vector<TripleVersions *> triples;
    uint32_t totalCount;
    bool hasExactCount;
    std::shared_ptr<DictionaryManager> dict;

public:
    SearchTriplesVersionWorker(OstrichStore *store, char *subject, char *predicate, char *object, uint32_t offset, uint32_t limit, Nan::Callback *callback, v8::Local<v8::Object> self)
            : Nan::AsyncWorker(callback),
              store(store), subject(subject), predicate(predicate), object(object),
              offset(offset), limit(limit), totalCount(0) {
        SaveToPersistent("self", self);
    };

    void Execute() {
        TripleVersionsIterator *it = nullptr;
        try {
            Controller *controller = store->GetController();

            // Prepare the triple pattern
            dict = controller->get_dictionary_manager(0);
            Triple triple_pattern(subject, predicate, toHdtLiteral(object), dict);

            // Estimate the total number of triples
            std::pair<size_t, hdt::ResultEstimationType> count_data = controller->get_version_count(triple_pattern, true);
            totalCount = count_data.first;
            it = controller->get_version(triple_pattern, offset);
            hasExactCount = count_data.second == hdt::EXACT;

            // Add matching triples to the result vector
            TripleVersions t;
            long count = 0;

            while (it->next(&t) && (!limit || triples.size() < limit)) {
                triples.push_back(new TripleVersions(new Triple(*t.get_triple()), new std::vector<int>(*t.get_versions())));
                count++;
            }
        } catch (const std::runtime_error& error) {
            SetErrorMessage(error.what());
        }
        delete it;
    }

    void HandleOKCallback() {
        Nan::HandleScope scope;

        // Convert the triples into a JavaScript object array
        uint32_t count = 0;
        v8::Local<v8::Array> triplesArray = Nan::New<v8::Array>(triples.size());
        const v8::Local<v8::String> SUBJECT = Nan::New("subject").ToLocalChecked();
        const v8::Local<v8::String> PREDICATE = Nan::New("predicate").ToLocalChecked();
        const v8::Local<v8::String> OBJECT = Nan::New("object").ToLocalChecked();
        const v8::Local<v8::String> VERSIONS = Nan::New("versions").ToLocalChecked();
        for (auto& t: triples) {
            v8::Local<v8::Object> tripleObject = Nan::New<v8::Object>();
            tripleObject->Set(Nan::GetCurrentContext(), SUBJECT, Nan::New(t->get_triple()->get_subject(*dict).c_str()).ToLocalChecked());
            tripleObject->Set(Nan::GetCurrentContext(), PREDICATE, Nan::New(t->get_triple()->get_predicate(*dict).c_str()).ToLocalChecked());
            std::string object = t->get_triple()->get_object(*dict);
            tripleObject->Set(Nan::GetCurrentContext(), OBJECT, Nan::New(fromHdtLiteral(object).c_str()).ToLocalChecked());

            v8::Local<v8::Array> versionsArray = Nan::New<v8::Array>(t->get_versions()->size());
            for (uint32_t countVersions = 0; countVersions < t->get_versions()->size(); countVersions++) {
                versionsArray->Set(Nan::GetCurrentContext(), countVersions, Nan::New((*(t->get_versions()))[countVersions]));
            }
            tripleObject->Set(Nan::GetCurrentContext(), VERSIONS, versionsArray);
            triplesArray->Set(Nan::GetCurrentContext(), count++, tripleObject);
            delete t->get_triple();
            delete t->get_versions();
        }

        // Send the JavaScript array and estimated total count through the callback
        const unsigned argc = 4;
        v8::Local<v8::Value> argv[argc] = {Nan::Null(), triplesArray, Nan::New<v8::Integer>((uint32_t) totalCount), Nan::New<v8::Boolean>((bool) hasExactCount)};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), argc, argv);
    }

    void HandleErrorCallback() {
        Nan::HandleScope scope;
        v8::Local<v8::Value> argv[] = {v8::Exception::Error(Nan::New(ErrorMessage()).ToLocalChecked())};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), 1, argv);
    }
};

// Searches for a triple pattern in the document.
// JavaScript signature: OstrichStore#_searchTriplesVersion(subject, predicate, object, offset, limit, callback)
NAN_METHOD(OstrichStore::SearchTriplesVersion) {
    assert(info.Length() == 8);
    Nan::AsyncQueueWorker(new SearchTriplesVersionWorker(Unwrap<OstrichStore>(info.This()),
                                                         *Nan::Utf8String(info[0]), *Nan::Utf8String(info[1]),
                                                         *Nan::Utf8String(info[2]),
                                                         info[3]->Uint32Value(Nan::GetCurrentContext()).FromJust(),
                                                         info[4]->Uint32Value(Nan::GetCurrentContext()).FromJust(),
                                                         new Nan::Callback(info[5].As<v8::Function>()),
                                                         info[6]->IsObject() ? info[6].As<v8::Object>() : info.This()));
}

/******** OstrichStore#_append ********/

class AppendWorker : public Nan::AsyncWorker {
    OstrichStore *store;
    int version;
    PatchElementIteratorVector *it_patch = nullptr;
    IteratorTripleStringVector *it_snapshot = nullptr;
    std::vector<PatchElement> *elements_patch;
    std::vector<hdt::TripleString> *elements_snapshot;
    v8::Persistent<v8::Object> self;
    std::shared_ptr<DictionaryManager> dict;
    uint32_t insertedCount = 0;

public:
    AppendWorker(OstrichStore *store, int version, v8::Local<v8::Array> triples, Nan::Callback *callback, v8::Local<v8::Object> self)
            : Nan::AsyncWorker(callback), store(store) {
        SaveToPersistent("self", self);
// For lower memory usage, we would have to use the (streaming) patch builder.
        try {
            Controller *controller = store->GetController();

            // Check version
            this->version = version >= 0 ? version : controller->get_max_patch_id() + 1;

            const v8::Local<v8::String> SUBJECT = Nan::New("subject").ToLocalChecked();
            const v8::Local<v8::String> PREDICATE = Nan::New("predicate").ToLocalChecked();
            const v8::Local<v8::String> OBJECT = Nan::New("object").ToLocalChecked();
            const v8::Local<v8::String> ADDITION = Nan::New("addition").ToLocalChecked();

            // Prepare the iterator
            elements_patch = new std::vector<PatchElement>();
            elements_snapshot = new std::vector<hdt::TripleString>();
            if (version == 0) {
                for (uint32_t i = 0; i < triples->Length(); i++) {
                    v8::Local<v8::Object> tripleObject = triples->Get(Nan::GetCurrentContext(), i).ToLocalChecked()->ToObject(Nan::GetCurrentContext()).ToLocalChecked();
                    std::string subject = std::string(*v8::String::Utf8Value(v8::Isolate::GetCurrent(), tripleObject->Get(Nan::GetCurrentContext(), SUBJECT).ToLocalChecked()->ToString(Nan::GetCurrentContext()).ToLocalChecked()));
                    std::string predicate = std::string(*v8::String::Utf8Value(v8::Isolate::GetCurrent(), tripleObject->Get(Nan::GetCurrentContext(), PREDICATE).ToLocalChecked()->ToString(Nan::GetCurrentContext()).ToLocalChecked()));
                    std::string object = std::string(*v8::String::Utf8Value(v8::Isolate::GetCurrent(), tripleObject->Get(Nan::GetCurrentContext(), OBJECT).ToLocalChecked()->ToString(Nan::GetCurrentContext()).ToLocalChecked()));
                    bool addition = tripleObject->Get(Nan::GetCurrentContext(), ADDITION).ToLocalChecked()->BooleanValue(v8::Isolate::GetCurrent());
                    if (!addition) {
                        SetErrorMessage("All triples of the initial snapshot MUST be additions, but a deletion was found.");
                        it_snapshot = nullptr;
                        break;
                    }
                    elements_snapshot->push_back(hdt::TripleString(subject, predicate, object));
                }
                it_snapshot = new IteratorTripleStringVector(elements_snapshot);
            } else {
                dict = controller->get_dictionary_manager(0);
                for (uint32_t i = 0; i < triples->Length(); i++) {
                    v8::Local<v8::Object> tripleObject = triples->Get(Nan::GetCurrentContext(), i).ToLocalChecked()->ToObject(Nan::GetCurrentContext()).ToLocalChecked();
                    std::string subject = std::string(*v8::String::Utf8Value(v8::Isolate::GetCurrent(), tripleObject->Get(Nan::GetCurrentContext(), SUBJECT).ToLocalChecked()->ToString(Nan::GetCurrentContext()).ToLocalChecked()));
                    std::string predicate = std::string(*v8::String::Utf8Value(v8::Isolate::GetCurrent(), tripleObject->Get(Nan::GetCurrentContext(), PREDICATE).ToLocalChecked()->ToString(Nan::GetCurrentContext()).ToLocalChecked()));
                    std::string object = std::string(*v8::String::Utf8Value(v8::Isolate::GetCurrent(), tripleObject->Get(Nan::GetCurrentContext(), OBJECT).ToLocalChecked()->ToString(Nan::GetCurrentContext()).ToLocalChecked()));
                    bool addition = tripleObject->Get(Nan::GetCurrentContext(), ADDITION).ToLocalChecked()->BooleanValue(v8::Isolate::GetCurrent());
                    PatchElement element(Triple(subject, predicate, object, dict), addition);
                    elements_patch->push_back(element);
                    insertedCount++;
                }
                it_patch = new PatchElementIteratorVector(elements_patch);
            }
        } catch (const runtime_error& error) {
            SetErrorMessage(error.what());
        }
    };

    void Execute() {
        try {
            // Insert
            Controller *controller = store->GetController();
            if (it_patch) {
                controller->append(it_patch, version, dict, false); // For debugging, add: new StdoutProgressListener()
            } else if (it_snapshot) {
                std::cout.setstate(std::ios_base::failbit); // Disable cout info from HDT
                std::shared_ptr<hdt::HDT> hdt = controller->get_snapshot_manager()->create_snapshot(version, it_snapshot, "<http://example.org>");
                std::cout.clear();
                insertedCount = hdt->getTriples()->getNumberOfElements();
            }
            delete elements_patch;
            delete elements_snapshot;
        }
        catch (const runtime_error& error) {
            SetErrorMessage(error.what());
        }
        delete it_patch;
        delete it_snapshot;
    }

    void HandleOKCallback() {
        Nan::HandleScope scope;

        // Send the JavaScript array and estimated total count through the callback
        const unsigned argc = 2;
        v8::Local<v8::Value> argv[argc] = {Nan::Null(), Nan::New<v8::Integer>(insertedCount)};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), argc, argv);
    }

    void HandleErrorCallback() {
        Nan::HandleScope scope;
        v8::Local<v8::Value> argv[] = {v8::Exception::Error(Nan::New(ErrorMessage()).ToLocalChecked())};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), 1, argv);
    }
};

// JavaScript signature: OstrichStore#_append(version, triples, callback, self)
NAN_METHOD(OstrichStore::Append) {
    assert(info.Length() == 8);
    Nan::AsyncQueueWorker(new AppendWorker(Unwrap<OstrichStore>(info.This()),
                                           info[0]->Uint32Value(Nan::GetCurrentContext()).FromJust(),
                                           info[1].As<v8::Array>(),
                                           new Nan::Callback(info[2].As<v8::Function>()),
                                           info[3]->IsObject() ? info[3].As<v8::Object>() : info.This()));
}


/******** OstrichStore#maxVersion ********/


// The max version that is available in the dataset
NAN_PROPERTY_GETTER(OstrichStore::MaxVersion) {
    auto *ostrichStore = Unwrap<OstrichStore>(info.This());
    info.GetReturnValue().Set(Nan::New<v8::Integer>(ostrichStore->GetController()->get_max_patch_id()));
}

/******** OstrichStore#features ********/


// Gets a bitvector indicating the supported features.
NAN_PROPERTY_GETTER(OstrichStore::Features) {
    auto *ostrichStore = Unwrap<OstrichStore>(info.This());
    info.GetReturnValue().Set(Nan::New<v8::Integer>(ostrichStore->features));
}



/******** OstrichStore#close ********/

// Closes the document, disabling all further operations.
// JavaScript signature: OstrichStore#_close([remove] [callback], [self])
NAN_METHOD(OstrichStore::Close) {
    int argcOffset = 0;
    bool remove = false;
    if (info.Length() >= 1 && info[0]->IsBoolean()) {
        argcOffset = 1;
        remove = info[0]->BooleanValue(v8::Isolate::GetCurrent());
    }

    // Destroy the current store
    OstrichStore *ostrichStore = Unwrap<OstrichStore>(info.This());
    ostrichStore->Destroy(remove);

    // Call the callback if one was passed
    if (info.Length() >= 1 + argcOffset && info[argcOffset]->IsFunction()) {
        const v8::Local<v8::Function> callback = info[argcOffset].As<v8::Function>();
        const v8::Local<v8::Object> self = (info.Length() >= 2 + argcOffset && info[1 + argcOffset]->IsObject()) ? info[1].As<v8::Object>() : Nan::GetCurrentContext()->Global();
        const unsigned argc = 1;
        v8::Handle<v8::Value> argv[argc] = {Nan::Null()};
        callback->Call(Nan::GetCurrentContext(), self, argc, argv);
    }
}



/******** OstrichStore#closed ********/


// Gets a boolean indicating whether the document is closed.
NAN_PROPERTY_GETTER(OstrichStore::Closed) {
    auto *ostrichStore = Unwrap<OstrichStore>(info.This());
    info.GetReturnValue().Set(Nan::New<v8::Boolean>(!ostrichStore->controller));
}



/******** Utility functions ********/


// The JavaScript representation for a literal with a datatype is
//   "literal"^^http://example.org/datatype
// whereas the HDT representation is
//   "literal"^^<http://example.org/datatype>
// The functions below convert when needed.


// Converts a JavaScript literal to an HDT literal
std::string &toHdtLiteral(std::string &literal) {
    // Check if the object is a literal with a datatype, which needs conversion
    std::string::const_iterator obj;
    std::string::iterator objLast;
    if (*(obj = literal.begin()) == '"' && *(objLast = literal.end() - 1) != '"') {
        // If the start of a datatype was found, surround it with angular brackets
        std::string::const_iterator datatype = objLast;
        while (obj != --datatype && *datatype != '@' && *datatype != '^');
        if (*datatype == '^') {
            // Allocate space for brackets, and update iterators
            literal.resize(literal.length() + 2);
            datatype += (literal.begin() - obj) + 1;
            objLast = literal.end() - 1;
            // Add brackets
            *objLast = '>';
            while (--objLast != datatype)
                *objLast = *(objLast - 1);
            *objLast = '<';
        }
    }
    return literal;
}

// Converts an HDT literal to a JavaScript literal
std::string &fromHdtLiteral(std::string &literal) {
    // Check if the literal has a datatype, which needs conversion
    std::string::const_iterator obj;
    std::string::iterator objLast;
    if (*(obj = literal.begin()) == '"' && *(objLast = literal.end() - 1) == '>') {
        // Find the start of the datatype
        std::string::iterator datatype = objLast;
        while (obj != --datatype && *datatype != '<');
        // Change the datatype representation by removing angular brackets
        if (*datatype == '<')
            literal.erase(datatype), literal.erase(objLast - 1);
    }
    return literal;
}
