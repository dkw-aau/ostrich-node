#include "LiteralsUtils.h"
#include "BufferedOstrichStore.h"

#include <utility>


class VMNextWorker: public Nan::AsyncWorker {
private:
    TripleIterator* it;
    int32_t number;
    std::shared_ptr<DictionaryManager> dict;

    // Callback return values
    std::vector<Triple> triples;
    bool done;

public:
    VMNextWorker(TripleIterator *iterator, std::shared_ptr<DictionaryManager> dict, int32_t number, Nan::Callback *callback, v8::Local<v8::Object> self)
            : Nan::AsyncWorker(callback), it(iterator), number(number), dict(std::move(dict)), done(false) {
        SaveToPersistent("self", self);
    }

    void Execute() override {
        try {
            Triple t;
            uint32_t count = 0;
            while (count < number && it->next(&t)) {
                triples.push_back(t);
                count++;
            }
            if (count < number) {  // if count < number, it means that the iterator is finished
                done = true;
            }
        } catch (const std::runtime_error &error) {
            SetErrorMessage(error.what());
        }
    }

    void HandleOKCallback() override {
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
            triplesArray->Set(Nan::GetCurrentContext(), count++, tripleObject);
        }

        // Send the Javascript Array and whether we are done iterating
        const unsigned argc = 3;
        v8::Local<v8::Value> argv[argc] = {Nan::Null(), triplesArray, Nan::New<v8::Boolean>(done)};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), argc, argv);
    }

    void HandleErrorCallback() override {
        Nan::HandleScope scope;
        v8::Local<v8::Value> argv[] = {v8::Exception::Error(Nan::New(ErrorMessage()).ToLocalChecked())};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), 1, argv);
    }
};


// VersionMaterializationProcessor
Nan::Persistent<v8::Function> VersionMaterializationProcessor::constructor;

VersionMaterializationProcessor::VersionMaterializationProcessor(TripleIterator *vm_iterator,  std::shared_ptr<DictionaryManager> dict, const v8::Local<v8::Object> &handle)
        : iterator(vm_iterator), dict(std::move(dict)) {
    this->Wrap(handle);
}

void VersionMaterializationProcessor::Next(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.Length() >= 2);
    auto proc = Nan::ObjectWrap::Unwrap<VersionMaterializationProcessor>(info.This());
    Nan::AsyncQueueWorker(new VMNextWorker(proc->iterator.get(),
                                           proc->dict,
                                           info[0]->Int32Value(Nan::GetCurrentContext()).FromJust(),
                                           new Nan::Callback(info[1].As<v8::Function>()),
                                           info[2]->IsObject() ? info[2].As<v8::Object>() : info.This()));
}

void VersionMaterializationProcessor::New(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.IsConstructCall());
    info.GetReturnValue().Set(info.This());
}

const Nan::Persistent<v8::Function> &VersionMaterializationProcessor::GetConstructor() {
    if (constructor.IsEmpty()) {
        // Create constructor template
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
        tpl->SetClassName(Nan::New("VersionMaterializationProcessor").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);
        // Create prototype
        Nan::SetPrototypeMethod(tpl, "_next", Next);
        // Set constructor
        constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
    }
    return constructor;
}

/**
 * Async Worker for DeltaMaterializationProcessor::Next
 */
class DMNextWorker: public Nan::AsyncWorker {
private:
    TripleDeltaIterator* it;
    int32_t number;

    // Callback return values
    v8::Local<v8::Array> triplesArray;

public:
    DMNextWorker(TripleDeltaIterator *iterator, int32_t number, Nan::Callback *callback, v8::Local<v8::Object> self)
            : Nan::AsyncWorker(callback), it(iterator), number(number), triplesArray(Nan::New<v8::Array>(number)) {
        SaveToPersistent("self", self);
    }

    void Execute() override {
        try {
            TripleDelta triple;
            uint32_t count = 0;
            const v8::Local<v8::String> SUBJECT = Nan::New("subject").ToLocalChecked();
            const v8::Local<v8::String> PREDICATE = Nan::New("predicate").ToLocalChecked();
            const v8::Local<v8::String> OBJECT = Nan::New("object").ToLocalChecked();
            const v8::Local<v8::String> ADDITION = Nan::New("addition").ToLocalChecked();
            while (count < number && it->next(&triple)) {
                Triple* t = triple.get_triple();
                std::shared_ptr<DictionaryManager> dict = triple.get_dictionary();
                v8::Local<v8::Object> tripleObject = Nan::New<v8::Object>();
                tripleObject->Set(Nan::GetCurrentContext(), SUBJECT, Nan::New(t->get_subject(*dict).c_str()).ToLocalChecked());
                tripleObject->Set(Nan::GetCurrentContext(), PREDICATE, Nan::New(t->get_predicate(*dict).c_str()).ToLocalChecked());
                std::string object = t->get_object(*dict);
                tripleObject->Set(Nan::GetCurrentContext(), OBJECT, Nan::New(fromHdtLiteral(object).c_str()).ToLocalChecked());
                tripleObject->Set(Nan::GetCurrentContext(), ADDITION, Nan::New(triple.is_addition()));
                triplesArray->Set(Nan::GetCurrentContext(), count++, tripleObject);
            }
        } catch (const std::runtime_error &error) {
            SetErrorMessage(error.what());
        }
    }

    void HandleOKCallback() override {
        // Send the Javascript Array
        const unsigned argc = 2;
        v8::Local<v8::Value> argv[argc] = {Nan::Null(), triplesArray};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), argc, argv);
    }

    void HandleErrorCallback() override {
        Nan::HandleScope scope;
        v8::Local<v8::Value> argv[] = {v8::Exception::Error(Nan::New(ErrorMessage()).ToLocalChecked())};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), 1, argv);
    }
};

// DeltaMaterializationProcessor
Nan::Persistent<v8::Function> DeltaMaterializationProcessor::constructor;

DeltaMaterializationProcessor::DeltaMaterializationProcessor(TripleDeltaIterator *dm_iterator, const v8::Local<v8::Object> &handle) : iterator(dm_iterator) {
    this->Wrap(handle);
}

void DeltaMaterializationProcessor::Next(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.Length() >= 2);
    auto proc = Nan::ObjectWrap::Unwrap<DeltaMaterializationProcessor>(info.This());
    int bufferingSize = info[0]->Int32Value(Nan::GetCurrentContext()).FromJust();
    Nan::AsyncQueueWorker(new DMNextWorker(proc->iterator.get(),
                                           info[0]->Int32Value(Nan::GetCurrentContext()).FromJust(),
                                           new Nan::Callback(info[1].As<v8::Function>()),
                                           info[2]->IsObject() ? info[2].As<v8::Object>() : info.This()));
}

void DeltaMaterializationProcessor::New(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.IsConstructCall());
    info.GetReturnValue().Set(info.This());
}

const Nan::Persistent<v8::Function> &DeltaMaterializationProcessor::GetConstructor() {
    if (constructor.IsEmpty()) {
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
        tpl->SetClassName(Nan::New("DeltaMaterializationProcessor").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);
        // Set prototype
        Nan::SetPrototypeMethod(tpl, "_next", Next);
        // Set constructor
        constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
    }
    return constructor;
}




/**
 * Async Worker for VersionQueryProcessor::Next
 */
class VQNextWorker: public Nan::AsyncWorker {
private:
    TripleVersionsIterator* it;
    int32_t number;

    // Callback return values
    v8::Local<v8::Array> triplesArray;

public:
    VQNextWorker(TripleVersionsIterator *iterator, int32_t number, Nan::Callback *callback, v8::Local<v8::Object> self)
            : Nan::AsyncWorker(callback), it(iterator), number(number), triplesArray(Nan::New<v8::Array>(number)) {
        SaveToPersistent("self", self);
    }

    void Execute() override {
        try {
            TripleVersions triple;
            uint32_t count = 0;
            const v8::Local<v8::String> SUBJECT = Nan::New("subject").ToLocalChecked();
            const v8::Local<v8::String> PREDICATE = Nan::New("predicate").ToLocalChecked();
            const v8::Local<v8::String> OBJECT = Nan::New("object").ToLocalChecked();
            const v8::Local<v8::String> VERSIONS = Nan::New("versions").ToLocalChecked();
            while (count < number && it->next(&triple)) {
                std::shared_ptr<DictionaryManager> dict = triple.get_dictionary();
                v8::Local<v8::Object> tripleObject = Nan::New<v8::Object>();
                tripleObject->Set(Nan::GetCurrentContext(), SUBJECT, Nan::New(triple.get_triple()->get_subject(*dict).c_str()).ToLocalChecked());
                tripleObject->Set(Nan::GetCurrentContext(), PREDICATE, Nan::New(triple.get_triple()->get_predicate(*dict).c_str()).ToLocalChecked());
                std::string object = triple.get_triple()->get_object(*dict);
                tripleObject->Set(Nan::GetCurrentContext(), OBJECT, Nan::New(fromHdtLiteral(object).c_str()).ToLocalChecked());

                v8::Local<v8::Array> versionsArray = Nan::New<v8::Array>(triple.get_versions()->size());
                for (uint32_t countVersions = 0; countVersions < triple.get_versions()->size(); countVersions++) {
                    versionsArray->Set(Nan::GetCurrentContext(), countVersions, Nan::New((*(triple.get_versions()))[countVersions]));
                }
                tripleObject->Set(Nan::GetCurrentContext(), VERSIONS, versionsArray);
                triplesArray->Set(Nan::GetCurrentContext(), count++, tripleObject);
            }
        } catch (const std::runtime_error &error) {
            SetErrorMessage(error.what());
        }
    }

    void HandleOKCallback() override {
        // Send the Javascript Array and whether we are done iterating
        const unsigned argc = 2;
        v8::Local<v8::Value> argv[argc] = {Nan::Null(), triplesArray};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), argc, argv);
    }

    void HandleErrorCallback() override {
        Nan::HandleScope scope;
        v8::Local<v8::Value> argv[] = {v8::Exception::Error(Nan::New(ErrorMessage()).ToLocalChecked())};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), 1, argv);
    }
};

// VersionQueryProcessor
Nan::Persistent<v8::Function> VersionQueryProcessor::constructor;

VersionQueryProcessor::VersionQueryProcessor(TripleVersionsIterator *vq_iterator, const v8::Local<v8::Object> &handle) : iterator(vq_iterator) {
    this->Wrap(handle);
}

void VersionQueryProcessor::Next(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.Length() >= 2);
    auto proc = Nan::ObjectWrap::Unwrap<VersionQueryProcessor>(info.This());
    Nan::AsyncQueueWorker(new VQNextWorker(proc->iterator.get(),
                                           info[0]->Int32Value(Nan::GetCurrentContext()).FromJust(),
                                           new Nan::Callback(info[1].As<v8::Function>()),
                                           info[2]->IsObject() ? info[2].As<v8::Object>() : info.This()));
}

void VersionQueryProcessor::New(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.IsConstructCall());
    info.GetReturnValue().Set(info.This());
}

const Nan::Persistent<v8::Function> &VersionQueryProcessor::GetConstructor() {
    if (constructor.IsEmpty()) {
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
        tpl->SetClassName(Nan::New("DeltaMaterializationProcessor").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);
        // Set prototype
        Nan::SetPrototypeMethod(tpl, "_next", Next);
        // Set constructor
        constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
    }
    return constructor;
}


// ================================================================================
// ================================================================================
// ================================================================================

Nan::Persistent<v8::Function> BufferedOstrichStore::constructor;

// Creates a new Ostrich store.
BufferedOstrichStore::BufferedOstrichStore(std::string path, const v8::Local<v8::Object> &handle, Controller *controller) : path(std::move(path)), controller(controller), features(1) {
    this->Wrap(handle);
}

BufferedOstrichStore::~BufferedOstrichStore() {
    Destroy(false);
}

// Destroys the document, disabling all further operations.
void BufferedOstrichStore::Destroy(bool remove) {
    if (controller != nullptr) {
        if (remove) {
            Controller::cleanup(path, controller);
        } else {
            delete controller;
        }
        controller = nullptr;
    }
}

void BufferedOstrichStore::New(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.IsConstructCall());
    info.GetReturnValue().Set(info.This());
}

const Nan::Persistent<v8::Function> &BufferedOstrichStore::GetConstructor() {
    if (constructor.IsEmpty()) {
        // Create constructor template
        v8::Local<v8::FunctionTemplate> constructorTemplate = Nan::New<v8::FunctionTemplate>(New);
        constructorTemplate->SetClassName(Nan::New("BufferedOstrichStore").ToLocalChecked());
        constructorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
        // Create prototype
        Nan::SetPrototypeMethod(constructorTemplate, "_searchTriplesVersionMaterialized", SearchTriplesVersionMaterialized);
        Nan::SetPrototypeMethod(constructorTemplate, "_countTriplesVersionMaterialized", CountTriplesVersionMaterialized);
        Nan::SetPrototypeMethod(constructorTemplate, "_searchTriplesDeltaMaterialized", SearchTriplesDeltaMaterialized);
        Nan::SetPrototypeMethod(constructorTemplate, "_countTriplesDeltaMaterialized", CountTriplesDeltaMaterialized);
        Nan::SetPrototypeMethod(constructorTemplate, "_searchTriplesVersion", SearchTriplesVersion);
        Nan::SetPrototypeMethod(constructorTemplate, "_countTriplesVersion", CountTriplesVersion);
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
        v8::Local<v8::Object> newStore = Nan::NewInstance(Nan::New(BufferedOstrichStore::GetConstructor())).ToLocalChecked();
        new BufferedOstrichStore(path, newStore, controller);
        // Send the new OstrichStore through the callback
        const unsigned argc = 2;
        v8::Local<v8::Value> argv[argc] = {Nan::Null(), newStore};
        // callback->Call(argc, argv);
        Nan::Call(*callback, argc, argv);
    }
};

void BufferedOstrichStore::Create(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.Length() >= 4);
    Nan::AsyncQueueWorker(new CreateWorker(*Nan::Utf8String(info[0]),
                                           info[1]->BooleanValue(info.GetIsolate()),
                                           *Nan::Utf8String(info[2]),
                                           *Nan::Utf8String(info[3]),
                                           new Nan::Callback(info[4].As<v8::Function>())));
}

/******** SearchTriplesVersionMaterialized ********/

void BufferedOstrichStore::SearchTriplesVersionMaterialized(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.Length() >= 5);
    auto thisStore = Nan::ObjectWrap::Unwrap<BufferedOstrichStore>(info.This());

    std::string s(*Nan::Utf8String(info[0]));
    std::string p(*Nan::Utf8String(info[1]));
    std::string o(*Nan::Utf8String(info[2]));

    int offset = info[3]->Int32Value(Nan::GetCurrentContext()).FromJust();
    int version = info[4]->Int32Value(Nan::GetCurrentContext()).FromJust();

    TripleIterator* it = thisStore->GetController()->get_version_materialized(StringTriple(s, p, o), offset, version);
    std::shared_ptr<DictionaryManager> dict = thisStore->GetController()->get_dictionary_manager(version);

    v8::Local<v8::Object> queryProcessor = Nan::NewInstance(Nan::New(VersionMaterializationProcessor::GetConstructor())).ToLocalChecked();
    new VersionMaterializationProcessor(it, dict, queryProcessor);

    info.GetReturnValue().Set(queryProcessor);
}

/******** CountTriplesVersionMaterialized ********/

class CountTriplesVersionMaterializedWorker : public Nan::AsyncWorker {
    BufferedOstrichStore *store;
    // JavaScript function arguments
    std::string subject, predicate, object;
    int version;
    // Callback return values
    uint32_t totalCount{0};
    bool hasExactCount{false};

public:
    CountTriplesVersionMaterializedWorker(BufferedOstrichStore *store, std::string subject, std::string predicate, std::string object, int32_t version, Nan::Callback *callback, v8::Local<v8::Object> self)
            : Nan::AsyncWorker(callback), store(store), subject(std::move(subject)), predicate(std::move(predicate)), object(std::move(object)), version(version) {
        SaveToPersistent("self", self);
    };

    void Execute() override {
        try {
            Controller *controller = store->GetController();

            // Check version
            version = version >= 0 ? version : controller->get_max_patch_id();

            // Prepare the triple pattern
            StringTriple triple_pattern(subject, predicate, toHdtLiteral(object));

            // Estimate the total number of triples
            std::pair<size_t, hdt::ResultEstimationType> count_data = controller->get_version_materialized_count(triple_pattern, version, true);
            totalCount = count_data.first;
            hasExactCount = count_data.second == hdt::EXACT;
        } catch (const std::runtime_error &error) {
            SetErrorMessage(error.what());
        }
    }

    void HandleOKCallback() override {
        Nan::HandleScope scope;
        // Send the JavaScript array and estimated total count through the callback
        const unsigned argc = 3;
        v8::Local<v8::Value> argv[argc] = {Nan::Null(), Nan::New<v8::Integer>((uint32_t) totalCount), Nan::New<v8::Boolean>((bool) hasExactCount)};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), argc, argv);
    }

    void HandleErrorCallback() override {
        Nan::HandleScope scope;
        v8::Local<v8::Value> argv[] = {v8::Exception::Error(Nan::New(ErrorMessage()).ToLocalChecked())};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), 1, argv);
    }
};

void BufferedOstrichStore::CountTriplesVersionMaterialized(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.Length() >= 5);
    auto thisStore = Nan::ObjectWrap::Unwrap<BufferedOstrichStore>(info.This());

    std::string s(*Nan::Utf8String(info[0]));
    std::string p(*Nan::Utf8String(info[1]));
    std::string o(*Nan::Utf8String(info[2]));
    int version = info[3]->Int32Value(Nan::GetCurrentContext()).FromJust();
    auto callback = new Nan::Callback(info[4].As<v8::Function>());
    auto self = info[5]->IsObject() ? info[5].As<v8::Object>() : info.This();

    Nan::AsyncQueueWorker(new CountTriplesVersionMaterializedWorker(thisStore, s, p, o, version, callback, self));
}

/******** SearchTriplesDeltaMaterialized ********/

void BufferedOstrichStore::SearchTriplesDeltaMaterialized(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.Length() >= 5);
    auto thisStore = Nan::ObjectWrap::Unwrap<BufferedOstrichStore>(info.This());

    std::string s(*Nan::Utf8String(info[0]));
    std::string p(*Nan::Utf8String(info[1]));
    std::string o(*Nan::Utf8String(info[2]));

    int offset = info[3]->Int32Value(Nan::GetCurrentContext()).FromJust();
    int version_start = info[4]->Int32Value(Nan::GetCurrentContext()).FromJust();
    int version_end = info[5]->Int32Value(Nan::GetCurrentContext()).FromJust();

    TripleDeltaIterator* it = thisStore->GetController()->get_delta_materialized(StringTriple(s, p, o), offset, version_start, version_end);

    v8::Local<v8::Object> queryProcessor = Nan::NewInstance(Nan::New(DeltaMaterializationProcessor::GetConstructor())).ToLocalChecked();
    new DeltaMaterializationProcessor(it, queryProcessor);

    info.GetReturnValue().Set(queryProcessor);
}

/******** CountTriplesDeltaMaterialized ********/

class CountTriplesDeltaMaterializedWorker : public Nan::AsyncWorker {
    BufferedOstrichStore *store;
    // JavaScript function arguments
    std::string subject, predicate, object;
    int version_start, version_end;
    // Callback return values
    uint32_t totalCount{0};
    bool hasExactCount;

public:
    CountTriplesDeltaMaterializedWorker(BufferedOstrichStore *store, std::string subject, std::string predicate, std::string object,
                                        int32_t version_start, int32_t version_end, Nan::Callback *callback, v8::Local<v8::Object> self)
            : Nan::AsyncWorker(callback),
              store(store), subject(std::move(subject)), predicate(std::move(predicate)), object(std::move(object)),
              version_start(version_start), version_end(version_end) {
        SaveToPersistent("self", self);
    }

    void Execute() override {
        try {
            Controller *controller = store->GetController();

            // Check version
            version_end = version_end >= 0 ? version_end : controller->get_max_patch_id();

            // Prepare the triple pattern
            StringTriple triple_pattern(subject, predicate, toHdtLiteral(object));

            // Estimate the total number of triples
            std::pair<size_t, hdt::ResultEstimationType> count_data = controller->get_delta_materialized_count(triple_pattern, version_start, version_end, true);
            totalCount = count_data.first;
            hasExactCount = count_data.second == hdt::EXACT;
        } catch (const std::runtime_error &error) {
            SetErrorMessage(error.what());
        }
    }

    void HandleOKCallback() override {
        Nan::HandleScope scope;
        // Send the JavaScript array and estimated total count through the callback
        const unsigned argc = 3;
        v8::Local<v8::Value> argv[argc] = {Nan::Null(), Nan::New<v8::Integer>((uint32_t) totalCount), Nan::New<v8::Boolean>((bool) hasExactCount)};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), argc, argv);
    }

    void HandleErrorCallback() override {
        Nan::HandleScope scope;
        v8::Local<v8::Value> argv[] = {v8::Exception::Error(Nan::New(ErrorMessage()).ToLocalChecked())};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), 1, argv);
    }
};

void BufferedOstrichStore::CountTriplesDeltaMaterialized(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.Length() >= 7);
    auto thisStore = Nan::ObjectWrap::Unwrap<BufferedOstrichStore>(info.This());

    std::string s(*Nan::Utf8String(info[0]));
    std::string p(*Nan::Utf8String(info[1]));
    std::string o(*Nan::Utf8String(info[2]));
    int version_start = info[3]->Int32Value(Nan::GetCurrentContext()).FromJust();
    int version_end = info[4]->Int32Value(Nan::GetCurrentContext()).FromJust();
    auto callback = new Nan::Callback(info[5].As<v8::Function>());
    auto self = info[6]->IsObject() ? info[6].As<v8::Object>() : info.This();

    Nan::AsyncQueueWorker(new CountTriplesDeltaMaterializedWorker(thisStore, s, p, o, version_start, version_end, callback, self));
}

/******** SearchTriplesVersion ********/

void BufferedOstrichStore::SearchTriplesVersion(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.Length() >= 4);
    auto thisStore = Nan::ObjectWrap::Unwrap<BufferedOstrichStore>(info.This());

    std::string s(*Nan::Utf8String(info[0]));
    std::string p(*Nan::Utf8String(info[1]));
    std::string o(*Nan::Utf8String(info[2]));

    int offset = info[3]->Int32Value(Nan::GetCurrentContext()).FromJust();

    TripleVersionsIterator* it = thisStore->GetController()->get_version(StringTriple(s, p, o), offset);

    v8::Local<v8::Object> queryProcessor = Nan::NewInstance(Nan::New(VersionQueryProcessor::GetConstructor())).ToLocalChecked();
    new VersionQueryProcessor(it, queryProcessor);

    info.GetReturnValue().Set(queryProcessor);
}

/******** CountTriplesVersion ********/

class CountTriplesVersionWorker : public Nan::AsyncWorker {
    BufferedOstrichStore *store;
    // JavaScript function arguments
    std::string subject, predicate, object;
    // Callback return values
    uint32_t totalCount;
    bool hasExactCount;

public:
    CountTriplesVersionWorker(BufferedOstrichStore *store, std::string subject, std::string predicate, std::string object, Nan::Callback *callback, v8::Local<v8::Object> self)
            : Nan::AsyncWorker(callback),
              store(store), subject(std::move(subject)), predicate(std::move(predicate)), object(std::move(object)), totalCount(0) {
        SaveToPersistent("self", self);
    };

    void Execute() override {
        try {
            Controller *controller = store->GetController();

            // Prepare the triple pattern
            StringTriple triple_pattern(subject, predicate, toHdtLiteral(object));

            // Estimate the total number of triples
            std::pair<size_t, hdt::ResultEstimationType> count_data = controller->get_version_count(triple_pattern, true);
            totalCount = count_data.first;
            hasExactCount = count_data.second == hdt::EXACT;
        } catch (const std::runtime_error& error) {
            SetErrorMessage(error.what());
        }
    }

    void HandleOKCallback() override {
        Nan::HandleScope scope;

        // Send the JavaScript array and estimated total count through the callback
        const unsigned argc = 3;
        v8::Local<v8::Value> argv[argc] = {Nan::Null(), Nan::New<v8::Integer>((uint32_t) totalCount), Nan::New<v8::Boolean>((bool) hasExactCount)};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), argc, argv);
    }

    void HandleErrorCallback() override {
        Nan::HandleScope scope;
        v8::Local<v8::Value> argv[] = {v8::Exception::Error(Nan::New(ErrorMessage()).ToLocalChecked())};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), 1, argv);
    }
};

void BufferedOstrichStore::CountTriplesVersion(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.Length() >= 4);
    auto thisStore = Nan::ObjectWrap::Unwrap<BufferedOstrichStore>(info.This());

    std::string s(*Nan::Utf8String(info[0]));
    std::string p(*Nan::Utf8String(info[1]));
    std::string o(*Nan::Utf8String(info[2]));

    auto callback = new Nan::Callback(info[3].As<v8::Function>());
    auto self = info[4]->IsObject() ? info[4].As<v8::Object>() : info.This();

    Nan::AsyncQueueWorker(new CountTriplesVersionWorker(thisStore, s, p, o, callback, self));
}


/******** MaxVersion ********/

void BufferedOstrichStore::MaxVersion(v8::Local<v8::String> property, Nan::NAN_PROPERTY_GETTER_ARGS_TYPE info) {
    auto *ostrichStore = Nan::ObjectWrap::Unwrap<BufferedOstrichStore>(info.This());
    info.GetReturnValue().Set(Nan::New<v8::Integer>(ostrichStore->GetController()->get_max_patch_id()));
}

/******** Features ********/

void BufferedOstrichStore::Features(v8::Local<v8::String> property, Nan::NAN_PROPERTY_GETTER_ARGS_TYPE info) {
    auto *ostrichStore = Nan::ObjectWrap::Unwrap<BufferedOstrichStore>(info.This());
    info.GetReturnValue().Set(Nan::New<v8::Integer>(ostrichStore->features));
}

/******** Append ********/

class AppendWorker : public Nan::AsyncWorker {
    BufferedOstrichStore *store;
    int version;
    PatchElementIteratorVector *it_patch = nullptr;
    IteratorTripleStringVector *it_snapshot = nullptr;
    std::vector<PatchElement> *elements_patch;
    std::vector<hdt::TripleString> *elements_snapshot;
    std::shared_ptr<DictionaryManager> dict;
    uint32_t insertedCount = 0;

public:
    AppendWorker(BufferedOstrichStore *store, int version, v8::Local<v8::Array> triples, Nan::Callback *callback, v8::Local<v8::Object> self)
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

    void Execute() override {
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

    void HandleOKCallback() override {
        Nan::HandleScope scope;

        // Send the JavaScript array and estimated total count through the callback
        const unsigned argc = 2;
        v8::Local<v8::Value> argv[argc] = {Nan::Null(), Nan::New<v8::Integer>(insertedCount)};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), argc, argv);
    }

    void HandleErrorCallback() override {
        Nan::HandleScope scope;
        v8::Local<v8::Value> argv[] = {v8::Exception::Error(Nan::New(ErrorMessage()).ToLocalChecked())};
        Nan::Call(*callback, GetFromPersistent("self")->ToObject(Nan::GetCurrentContext()).ToLocalChecked(), 1, argv);
    }
};

void BufferedOstrichStore::Append(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.Length() >= 3);
    auto thisStore = Nan::ObjectWrap::Unwrap<BufferedOstrichStore>(info.This());

    int version =  info[0]->Int32Value(Nan::GetCurrentContext()).FromJust();
    auto triples = info[1].As<v8::Array>();
    auto callback = new Nan::Callback(info[2].As<v8::Function>());
    auto self = info[3]->IsObject() ? info[3].As<v8::Object>() : info.This();

    Nan::AsyncQueueWorker(new AppendWorker(thisStore, version, triples, callback, self));
}

/******** Close ********/

void BufferedOstrichStore::Close(Nan::NAN_METHOD_ARGS_TYPE info) {
    int argcOffset = 0;
    bool remove = false;
    if (info.Length() >= 1 && info[0]->IsBoolean()) {
        argcOffset = 1;
        remove = info[0]->BooleanValue(v8::Isolate::GetCurrent());
    }

    // Destroy the current store
    auto ostrichStore = Nan::ObjectWrap::Unwrap<BufferedOstrichStore>(info.This());
    ostrichStore->Destroy(remove);

    // Call the callback if one was passed
    if (info.Length() >= 1 + argcOffset && info[argcOffset]->IsFunction()) {
        const v8::Local<v8::Function> callback = info[argcOffset].As<v8::Function>();
        const v8::Local<v8::Object> self = (info.Length() >= 2 + argcOffset && info[1 + argcOffset]->IsObject()) ? info[1].As<v8::Object>() : Nan::GetCurrentContext()->Global();
        const unsigned argc = 1;
        v8::Handle<v8::Value> argv[argc] = {Nan::Null()};
//        callback->Call(Nan::GetCurrentContext(), self, argc, argv);
        Nan::Call(callback, self, argc, argv);
    }
}

/******** Closed ********/

void BufferedOstrichStore::Closed(v8::Local<v8::String> property, Nan::NAN_PROPERTY_GETTER_ARGS_TYPE info) {
    auto *ostrichStore = Nan::ObjectWrap::Unwrap<BufferedOstrichStore>(info.This());
    info.GetReturnValue().Set(Nan::New<v8::Boolean>(!ostrichStore->controller));
}
