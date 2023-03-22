#include <nan.h>
#include "BufferedOstrichStore.h"

NAN_MODULE_INIT(InitOstrichModule) {
    Nan::Set(target, Nan::New("BufferedOstrichStore").ToLocalChecked(),
             Nan::New(BufferedOstrichStore::GetConstructor()));
    Nan::Set(target, Nan::New("createBufferedOstrichStore").ToLocalChecked(),
             Nan::GetFunction(Nan::New<v8::FunctionTemplate>(BufferedOstrichStore::Create)).ToLocalChecked());
    Nan::Set(target, Nan::New("VersionMaterializationProcessor").ToLocalChecked(),
             Nan::New(VersionMaterializationProcessor::GetConstructor()));
    Nan::Set(target, Nan::New("DeltaMaterializationProcessor").ToLocalChecked(),
             Nan::New(DeltaMaterializationProcessor::GetConstructor()));
    Nan::Set(target, Nan::New("VersionQueryProcessor").ToLocalChecked(),
             Nan::New(VersionQueryProcessor::GetConstructor()));
}

NODE_MODULE(ostrich, InitOstrichModule)
