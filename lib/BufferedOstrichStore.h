#ifndef OSTRICH_BUFFEREDOSTRICHSTORE_H
#define OSTRICH_BUFFEREDOSTRICHSTORE_H

#include <memory>
#include <nan.h>

#include "../deps/ostrich/src/main/cpp/controller/controller.h"


class VersionMaterializationProcessor: public Nan::ObjectWrap {
private:
    std::unique_ptr<TripleIterator> iterator;
    std::shared_ptr<DictionaryManager> dict;

    static NAN_METHOD(New);
    // VersionMaterializationProcessor::next(number, callback, self)
    static NAN_METHOD(Next);

    static Nan::Persistent<v8::Function> constructor;
public:
    VersionMaterializationProcessor(TripleIterator* vm_iterator, std::shared_ptr<DictionaryManager> dict, const v8::Local<v8::Object> &handle);

    static const Nan::Persistent<v8::Function> &GetConstructor();
};


class DeltaMaterializationProcessor: public Nan::ObjectWrap {
private:
    std::unique_ptr<TripleDeltaIterator> iterator;

    static NAN_METHOD(New);
    // DeltaMaterializationProcessor::next(number, callback, self)
    static NAN_METHOD(Next);

    static Nan::Persistent<v8::Function> constructor;

public:
    explicit DeltaMaterializationProcessor(TripleDeltaIterator* dm_iterator, const v8::Local<v8::Object> &handle);

    static const Nan::Persistent<v8::Function> &GetConstructor();
};


class VersionQueryProcessor: public Nan::ObjectWrap {
private:
    std::unique_ptr<TripleVersionsIterator> iterator;

    static NAN_METHOD(New);
    // VersionQueryProcessor::next(number, callback, self)
    static NAN_METHOD(Next);

    static Nan::Persistent<v8::Function> constructor;

public:
    explicit VersionQueryProcessor(TripleVersionsIterator* vq_iterator, const v8::Local<v8::Object> &handle);

    static const Nan::Persistent<v8::Function> &GetConstructor();
};


class BufferedOstrichStore: public Nan::ObjectWrap {
private:
    Controller *controller;
    int features;
    std::string path;

    // Construction and destruction
    ~BufferedOstrichStore() override;

    void Destroy(bool remove);

    static NAN_METHOD(New);

    // OstrichStore#_searchTriplesVersionMaterialized(subject, predicate, object, offset, version, self)
    static NAN_METHOD(SearchTriplesVersionMaterialized);
    // OstrichStore#_countTriplesVersionMaterialized(subject, predicate, object, version, callback, self)
    static NAN_METHOD(CountTriplesVersionMaterialized);

    // OstrichStore#_searchTriplesDeltaMaterialized(subject, predicate, object, offset, version_start, version_end, self)
    static NAN_METHOD(SearchTriplesDeltaMaterialized);
    // OstrichStore#_countTriplesDeltaMaterialized(subject, predicate, object, version_start, version_end, callback, self)
    static NAN_METHOD(CountTriplesDeltaMaterialized);

    // OstrichStore#_searchTriplesVersion(subject, predicate, object, offset, self)
    static NAN_METHOD(SearchTriplesVersion);
    // OstrichStore#_countTriplesVersion(subject, predicate, object, callback, self)
    static NAN_METHOD(CountTriplesVersion);

    // OstrichStore#maxVersion
    static NAN_PROPERTY_GETTER(MaxVersion);

    // OstrichStore#_append(version, triples, callback, self)
    static NAN_METHOD(Append);

    // OstrichStore#_features
    static NAN_PROPERTY_GETTER(Features);

    // OstrichStore#_close([remove], [callback], [self])
    static NAN_METHOD(Close);

    // OstrichStore#closed
    static NAN_PROPERTY_GETTER(Closed);

    static Nan::Persistent<v8::Function> constructor;

public:
    BufferedOstrichStore(std::string path, const v8::Local<v8::Object> &handle, Controller *controller);

    static NAN_METHOD(Create);

    // static void Create(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static const Nan::Persistent<v8::Function> &GetConstructor();

    // Accessors
    Controller *GetController() { return controller; }
};


#endif //OSTRICH_BUFFEREDOSTRICHSTORE_H
