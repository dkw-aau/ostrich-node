#ifndef OstrichStore_H
#define OstrichStore_H

#include <node.h>
#include <nan.h>

#include "../deps/ostrich/src/main/cpp/controller/controller.h"

enum OstrichStoreFeatures {
    Versioning = 1, // The document supports versioning
};

class OstrichStore : public Nan::ObjectWrap {
public:
    OstrichStore(std::string path, const v8::Local<v8::Object> &handle, Controller *controller);

    static NAN_METHOD(Create);

    // static void Create(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static const Nan::Persistent<v8::Function> &GetConstructor();

    // Accessors
    Controller *GetController() { return controller; }

    [[nodiscard]] bool Supports(OstrichStoreFeatures feature) const {
        return features & (int) feature;
    }

private:
    Controller *controller;
    int features;
    std::string path;

    // Construction and destruction
    ~OstrichStore() override;

    void Destroy(bool remove);

    static NAN_METHOD(New);

    // OstrichStore#_searchTriplesVersionMaterialized(subject, predicate, object, offset, limit, version, callback, self)
    static NAN_METHOD(SearchTriplesVersionMaterialized);
    // OstrichStore#_countTriplesVersionMaterialized(subject, predicate, object, version, callback, self)
    static NAN_METHOD(CountTriplesVersionMaterialized);

    // OstrichStore#_searchTriplesDeltaMaterialized(subject, predicate, object, offset, limit, version_start, version_end, callback, self)
    static NAN_METHOD(SearchTriplesDeltaMaterialized);
    // OstrichStore#_countTriplesDeltaMaterialized(subject, predicate, object, version_start, version_end, callback, self)
    static NAN_METHOD(CountTriplesDeltaMaterialized);

    // OstrichStore#_searchTriplesVersion(subject, predicate, object, offset, limit, callback, self)
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
};

#endif
