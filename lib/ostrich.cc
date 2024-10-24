#include <node.h>
#include <nan.h>
#include "OstrichStore.h"

NAN_MODULE_INIT(InitOstrichModule) {
    Nan::Set(target, Nan::New("OstrichStore").ToLocalChecked(),
             Nan::New(OstrichStore::GetConstructor()));
    Nan::Set(target, Nan::New("createOstrichStore").ToLocalChecked(),
             Nan::GetFunction(Nan::New<v8::FunctionTemplate>(OstrichStore::Create)).ToLocalChecked());
}

NODE_MODULE(ostrich, InitOstrichModule)
