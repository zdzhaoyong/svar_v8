#include <nan.h>
#include <Svar/Registry.h>

using namespace sv;
using namespace v8;

class SvarJS: public Nan::ObjectWrap{
public:
    SvarJS(Svar obj=Svar())
        : object(obj){}

    static Local<Value> getNodeClass(Svar src){
        SvarClass& cls=src.as<SvarClass>();

        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(Constructor,
                                                                             Nan::New<External>(&cls));
        tpl->SetClassName(Nan::New(cls.name()).ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        for(std::pair<std::string,Svar> kv:cls._attr){
              if(kv.second.isFunction()){
                  SvarFunction& func=kv.second.as<SvarFunction>();
                  if(func.is_method)
                      Nan::SetPrototypeMethod(tpl,kv.first.c_str(),MemberMethod,Nan::New<External>(&func));
                  else
                      Nan::SetPrototypeMethod(tpl,kv.first.c_str(),Method,Nan::New<External>(&func));
              }
              else if(kv.second.is<SvarClass::SvarProperty>()){
//                  Nan::SetAccessor();
              }
        }

        Local<Function> constructor = tpl->GetFunction(context).ToLocalChecked();

        cls._attr["__js_constructor__"]=std::make_shared<Nan::Persistent<v8::Function>>(constructor);

        return constructor;
    }

    static void Constructor(const Nan::FunctionCallbackInfo<Value>& args){
        v8::HandleScope handle_scope(Isolate::GetCurrent());
        SvarClass& cls = *(SvarClass*) args.Data().As<External>()->Value();
        Svar __init__ = cls.__init__;
        if(!__init__.isFunction()){
            SvarJS* obj = new SvarJS();
            obj->Wrap(args.This());
            args.GetReturnValue().Set(args.This());
            return ;
        }

        if (args.IsConstructCall()){
            SvarJS* obj = new SvarJS();
            try{
                std::vector<Svar> argv;
                for(int i=0;i<args.Length();++i)
                    argv.push_back(fromNode(args[i]));
                obj->object=__init__.as<SvarFunction>().Call(argv);
            }
            catch(SvarExeption& e){
            }
            obj->Wrap(args.This());
            args.GetReturnValue().Set(args.This());
        }
        else{
            auto& constructor=cls._attr["__js_constructor__"].as<Nan::Persistent<v8::Function>>();
            std::vector<Local<Value>> argv;
            for(int i=0;i<args.Length();++i)
                argv.push_back(args[i]);

            v8::Local<v8::Function> cons = Nan::New<v8::Function>(constructor);
            Local<Value> value=cons->NewInstance(Isolate::GetCurrent()->GetCurrentContext(),argv.size(), argv.data()).ToLocalChecked();
            args.GetReturnValue().Set(value);
        }
    }

    static void Method(const Nan::FunctionCallbackInfo<Value>& args){
        v8::HandleScope handle_scope(Isolate::GetCurrent());
        SvarFunction* func = (SvarFunction*)args.Data().As<External>()->Value();

        std::vector<Svar> argv;
        for(int i=0;i<args.Length();++i)
            argv.push_back(fromNode(args[i]));
        auto ret=func->Call(argv);
        args.GetReturnValue().Set(getNode(ret));
    }

    static void MemberMethod(const Nan::FunctionCallbackInfo<Value>& args){
        v8::HandleScope handle_scope(Isolate::GetCurrent());
        SvarFunction* func = (SvarFunction*)args.Data().As<External>()->Value();

        SvarJS* self = ObjectWrap::Unwrap<SvarJS>(args.Holder());

        std::vector<Svar> argv;
        argv.push_back(self->object);
        for(int i=0;i<args.Length();++i)
            argv.push_back(fromNode(args[i]));
        auto ret=func->Call(argv);
        args.GetReturnValue().Set(getNode(ret));
    }


    static std::string stdString(Local<Value> arg){
        Nan::Utf8String val(arg->ToString());
        return std::string(*val, val.length());
    }

    static Local<String> jString(std::string str){
        return Nan::New<String>(str).ToLocalChecked();
    }

    static Svar fromNode(const Local<Value>& n){
        if(n.IsEmpty()) return Svar();

        std::string typeName=stdString(n->TypeOf(Isolate::GetCurrent()));

        if("string"==typeName)
            return stdString(n);

        if("boolean"==typeName)
            return n->BooleanValue();

        if("number"==typeName)
            return n->NumberValue();

        if("object"==typeName)
        {
            if(n->IsArray()){
                v8::Local<v8::Array> arr = n.As<v8::Array>();
                std::vector<Svar>    val;

                // Length of arr is checked in checkType().
                for(uint32_t num = 0; num < arr->Length(); ++num) {
                    v8::Local<v8::Value> item;

                    if(Nan::Get(arr, num).ToLocal(&item)) {
                        val[num] = fromNode(item);
                    } else {
                        throw(std::runtime_error("Error converting array element"));
                    }
                }

                return(val);
            }
            return n->IsArray();
        }

        if("function"==typeName){
            std::shared_ptr<Nan::Persistent<Function>> func=std::make_shared<Nan::Persistent<Function>>(n.As<Function>());

            std::shared_ptr<SvarFunction> svarfunc=std::make_shared<SvarFunction>();

            svarfunc->_func=[func](std::vector<Svar>& args)->Svar{
                Nan::AsyncResource resource("nan:makeCallback");
                Nan::Persistent<Function>& con=*func;
                v8::Local<v8::Function> cons =Nan::New<v8::Function>(con);

                std::vector<Local<Value> > jsargs;
                for(Svar& arg:args)
                    jsargs.push_back(getNode(arg));
                return fromNode(resource.runInAsyncScope(Nan::GetCurrentContext()->Global(),
                                                         cons, jsargs.size(), jsargs.data()).ToLocalChecked());
            };
            svarfunc->do_argcheck=false;

            return svarfunc;
        };

    //      std::cerr<<"Unable to cast "<<typeName<<std::endl;

        return Svar(n);
    }

    static Local<Value> getNode(Svar src){
        if(src.is<Local<Value>>())
            return src.as<Local<Value>>();

        SvarClass* cls=src.classPtr();
        Svar func=(*cls)["getJs"];
        if(func.isFunction())
            return func.as<SvarFunction>().Call({src}).as<Local<Value>>();

        std::function<Local<Value>(Svar)> convert;

        if(src.is<bool>())
          convert=[](Svar src){return Nan::New<Boolean>(src.as<bool>());};
        else if(src.is<int>())
            convert=[](Svar src){return Nan::New<Number>(src.as<int>());};
        else if(src.is<double>())
            convert=[](Svar src){return Nan::New<Number>(src.as<double>());};
        else if(src.isUndefined())
            convert=[](Svar src){return Nan::Undefined();};
        else if(src.isNull())
            convert=[](Svar src){return Nan::Null();};

        else if(src.is<std::string>())
            convert=[](Svar src)->Local<Value>{
                std::string& str=src.as<std::string>();
                return Nan::New<String>(str).ToLocalChecked();
            };
        else if(src.is<SvarArray>())
            convert=[](Svar src)->Local<Value>{
                int count = src.size();
                v8::Local<v8::Array> arr = Nan::New<v8::Array>(count);

                for(int num = 0; num < count; ++num) {
                    arr->Set((uint32_t)num, getNode(src[num]));
                }

                return arr;
        };
        else if(src.is<SvarObject>())
            convert=[](Svar src){
            v8::Local<v8::Object> obj = Nan::New<v8::Object>();
            for (std::pair<std::string,Svar> kv : src) {
    //              std::cout<<kv.first<<":"<<kv.second;
                obj->Set(Nan::New<String>(kv.first).ToLocalChecked(),getNode(kv.second));
            }
            return obj;
        };
        else if(src.is<SvarFunction>())
            convert=[](Svar src)->Local<Value>{
                SvarFunction& fsvar=src.as<SvarFunction>();

                v8::Isolate* isolate = v8::Isolate::GetCurrent();
                v8::Local<v8::Context> context = isolate->GetCurrentContext();
                v8::Local<v8::FunctionTemplate> t = Nan::New<FunctionTemplate>(Method,
                                                                              Nan::New<External>(&fsvar));
                v8::Local<v8::String> fn_name = Nan::New<String>(fsvar.name).ToLocalChecked();
                v8::Local<v8::Function> fn = t->GetFunction(context).ToLocalChecked();
                fn->SetName(fn_name);
                return fn;
            };
        else if(src.is<SvarClass>())
            convert=&SvarJS::getNodeClass;
        else
            convert=[](Svar src){
                SvarClass& cls=*src.classPtr();
                auto js_constructor = cls._attr["__js_constructor__"];
                Local<Function> cons;
                if(js_constructor.isUndefined()){
                    cons=getNodeClass(src.classObject()).As<Function>();
                }
                else{
                    auto& constructor=js_constructor.as<Nan::Persistent<v8::Function>>();
                    cons = Nan::New<v8::Function>(constructor);
                }

                Local<Object> selfObj=cons->NewInstance(Isolate::GetCurrent()->GetCurrentContext()).ToLocalChecked();

                SvarJS* self = ObjectWrap::Unwrap<SvarJS>(selfObj);
                self->object=src;

                return selfObj;
            };

        cls->def("getJs",convert);

        return convert(src);
    }

    Svar   object;
};

void init (Local<Object> exports) {

    static Svar load=SvarFunction(&Registry::load);

    exports->Set(SvarJS::jString("load"),SvarJS::getNode(load));
}

#undef svar
NODE_MODULE(svar, init)
