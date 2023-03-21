#include <node.h>
#include <nan.h>
#include "OstrichStore.h"
#include "BufferedOstrichStore.h"

using namespace v8;

NAN_MODULE_INIT(InitOstrichModule) {
    Nan::Set(target, Nan::New("OstrichStore").ToLocalChecked(),
             Nan::New(OstrichStore::GetConstructor()));
    Nan::Set(target, Nan::New("createOstrichStore").ToLocalChecked(),
             Nan::GetFunction(Nan::New<FunctionTemplate>(OstrichStore::Create)).ToLocalChecked());
    Nan::Set(target, Nan::New("BufferedOstrichStore").ToLocalChecked(),
             Nan::New(BufferedOstrichStore::GetConstructor()));
    Nan::Set(target, Nan::New("createBufferedOstrichStore").ToLocalChecked(),
             Nan::GetFunction(Nan::New<FunctionTemplate>(BufferedOstrichStore::Create)).ToLocalChecked());
    Nan::Set(target, Nan::New("VersionMaterializationProcessor").ToLocalChecked(),
             Nan::New(VersionMaterializationProcessor::GetConstructor()));
    Nan::Set(target, Nan::New("DeltaMaterializationProcessor").ToLocalChecked(),
             Nan::New(DeltaMaterializationProcessor::GetConstructor()));
    Nan::Set(target, Nan::New("VersionQueryProcessor").ToLocalChecked(),
             Nan::New(VersionQueryProcessor::GetConstructor()));
}

NODE_MODULE(ostrich, InitOstrichModule)
