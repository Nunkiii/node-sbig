/*
  The Quarklib project.
  Written by Pierre Sprimont, <sprimont@iasfbo.inaf.it>, INAF/IASF-Bologna, 2014.
  This source-code is not free, but do what you want with it anyway.
*/


/*#include "tracking.hh"*/
#include <time.h>
#include "sbig.hh"

#include "../node-fits/qk/pngwriter.hh"
#include "../node-fits/math/jsmat.hh"

namespace sadira{

  using namespace v8;
  using namespace node;
  using namespace std;  
  using namespace qk;

  Persistent<Function> sbig::constructor;
  
  void sbig_cam::expo_complete(double pc){
    sb->new_event.lock();
    sb->event_id=14;
    sb->complete=pc;
    sb->new_event.broadcast();
    sb->new_event.unlock();
  }


  void sbig_cam::grab_complete(double pc){
    sb->new_event.lock();
    sb->event_id=15;
    sb->complete=pc;
    sb->new_event.broadcast();
    sb->new_event.unlock();
  }
  
  sbig::sbig():
    pcam(0),
    expt(this),
    infinite_loop(false),
    continue_expo(0){  
  }
  
  sbig::~sbig(){
    try{shutdown();} 
    catch(qk::exception& e){
      MERROR<< e.mess << endl;
    }
  }
  
  
  void sbig::New(const FunctionCallbackInfo<Value>& args){

    Isolate* isolate = args.GetIsolate();
    
    if (args.IsConstructCall()) {
      
      //HandleScope scope;
      
      sbig* obj = new sbig();
      //  obj->counter_ = args[0]->IsUndefined() ? 0 : args[0]->NumberValue();
      
      
      //if(!args[0]->IsUndefined()){
      //v8::String::Utf8Value s(args[0]->ToString());
      //obj->file_name=*s;
      //}

      //->Set(String::NewFromUtf8(isolate,"file_name"), args[0]);
      args.This()->Set(String::NewFromUtf8(isolate,"exptime"), Number::New(isolate, 0.5));
      args.This()->Set(String::NewFromUtf8(isolate,"nexpo"), Number::New(isolate, 5));
      
      Local<Function> jsu_cons = Local<Function>::New(isolate, jsmat<unsigned short>::constructor);
      Local<Function> jsf_cons = Local<Function>::New(isolate, jsmat<float>::constructor);
      
      args.This()->Set(String::NewFromUtf8(isolate,"last_image"), jsu_cons->NewInstance());
      args.This()->Set(String::NewFromUtf8(isolate,"last_image_float"), jsf_cons->NewInstance());
      obj->Wrap(args.This());
      
      
      
      args.GetReturnValue().Set(args.This());
      //return args.This();
      
    }else{
      const int argc = 1;
      Local<Value> argv[argc] = { args[0] };
      Local<Function> cons = Local<Function>::New(isolate, constructor);
      args.GetReturnValue().Set(cons->NewInstance(argc, argv));

    }

  }
  

  //Persistent<FunctionTemplate> sbig::s_cts;
  
  
  void sbig::set_temp_func(const FunctionCallbackInfo<Value>& args){

    Isolate* isolate = args.GetIsolate();
    
    if (args.Length() != 2) {
      isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Need setpoint info! 2 pars: (enabled, setpoint)")));
      return;
    }

    sbig* obj = ObjectWrap::Unwrap<sbig>(args.This());
    sbig_cam* cam=obj->pcam;
    
    if(!cam){
      isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Camera not connected!")));
      return;
    }

    short unsigned int cooling_enabled=args[0]->ToNumber()->Value();
    double setpoint=args[1]->ToNumber()->Value();
    PAR_ERROR res = CE_NO_ERROR;

    if ( (res = cam->SetTemperatureRegulation(cooling_enabled, setpoint) ) != CE_NO_ERROR ){
      isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Error setting CCD cooling!")));
      return;
    }
    
    args.GetReturnValue().Set(args.This());
  }
  
  
  void sbig::get_temp_func(const FunctionCallbackInfo<Value>& args){

    Isolate* isolate = args.GetIsolate();
    
    sbig* obj = ObjectWrap::Unwrap<sbig>(args.This());
    
    sbig_cam* cam=obj->pcam;

    if(!cam){
      isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Camera not connected!")));
      return;
    }


    v8::Handle<v8::Object> result = v8::Object::New(isolate);

    PAR_ERROR res = CE_NO_ERROR;
    double d,setpoint,cooling_power;
    // CCD Temperature
    short unsigned int cooling_enabled;
    

    if ( (res = cam->QueryTemperatureStatus(cooling_enabled, d, setpoint, cooling_power)) != CE_NO_ERROR ){
      isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Error getting CCD temperature!")));
    }

    result->Set(String::NewFromUtf8(isolate, "cooling"),v8::Number::New(isolate, cooling_enabled));
    result->Set(String::NewFromUtf8(isolate, "cooling_setpoint"),v8::Number::New(isolate, setpoint));
    result->Set(String::NewFromUtf8(isolate, "cooling_power"),v8::Number::New(isolate, cooling_power));
    result->Set(String::NewFromUtf8(isolate, "ccd_temp"),v8::Number::New(isolate, d));
    
    QueryTemperatureStatusResults qtsr;
    
    // Ambient Temperature
    if ( (res = cam->SBIGUnivDrvCommand(CC_QUERY_TEMPERATURE_STATUS, NULL, &qtsr)) != CE_NO_ERROR ){
      isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Error getting Ambient temperature!")));
      return;
    }

    d=cam->ADToDegreesC(qtsr.ambientThermistor, FALSE);
    result->Set(String::NewFromUtf8(isolate, "ambient_temp"),v8::Number::New(isolate, d));
    args.GetReturnValue().Set(result);

  }

  void sbig::init(Local<Object> target){
    
    Isolate* isolate=target->GetIsolate();
    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    
    tpl->SetClassName(String::NewFromUtf8(isolate, "sbig"));
    tpl->InstanceTemplate()->SetInternalFieldCount(6);

    // Prototype
    
    NODE_SET_PROTOTYPE_METHOD(tpl, "initialize", initialize_func); 
    NODE_SET_PROTOTYPE_METHOD(tpl, "shutdown", shutdown_func);
    NODE_SET_PROTOTYPE_METHOD(tpl, "start_exposure",start_exposure_func);
    NODE_SET_PROTOTYPE_METHOD(tpl, "stop_exposure", stop_exposure_func);
    NODE_SET_PROTOTYPE_METHOD(tpl, "get_temp", get_temp_func);
    NODE_SET_PROTOTYPE_METHOD(tpl, "set_temp", set_temp_func);

    target->Set(String::NewFromUtf8(isolate,"cam"), tpl->GetFunction());
    constructor.Reset(isolate, tpl->GetFunction());
    
  }
  
  void send_status(Isolate* isolate, v8::Local<v8::Function>& cb, const string& type, const string& message, const string& id=""){
    
    const unsigned argc = 1;

    v8::Handle<v8::Object> msg = v8::Object::New(isolate);
    msg->Set(String::NewFromUtf8(isolate, "type"),String::NewFromUtf8(isolate, type.c_str()));
    msg->Set(String::NewFromUtf8(isolate, "content"),String::NewFromUtf8(isolate, message.c_str()));  
    if(id!="")
      msg->Set(String::NewFromUtf8(isolate, "id"),String::NewFromUtf8(isolate, id.c_str()));  
    v8::Handle<v8::Value> msgv(msg);
    Handle<Value> argv[argc] = { msgv };

    cb->Call(isolate->GetCurrentContext()->Global(), argc, argv );    
  }

  
  void sbig::send_status_message(Isolate* isolate, const string& type, const string& message){
    const unsigned argc = 1;

    v8::Handle<v8::Object> msg = v8::Object::New(isolate);
    msg->Set(String::NewFromUtf8(isolate, "type"),String::NewFromUtf8(isolate, type.c_str()));
    msg->Set(String::NewFromUtf8(isolate, "content"),String::NewFromUtf8(isolate, message.c_str()));  

    v8::Handle<v8::Value> msgv(msg);
    Handle<Value> argv[argc] = { msgv };
    cb->Call(isolate->GetCurrentContext()->Global(), argc, argv );    
  }

  void sbig::shutdown_func(const FunctionCallbackInfo<Value>& args){
    Isolate* isolate = args.GetIsolate();
    
    sbig* obj = ObjectWrap::Unwrap<sbig>(args.This());

    Local<Function> cb = Local<Function>::Cast(args[0]);
    send_status(isolate, cb,"info","Camera driver unloading","init");
    
    try{
      obj->shutdown();
      send_status(isolate, cb,"success","Camera driver unloaded","init");
    }
    catch (qk::exception& e){
      send_status(isolate, cb,"error",e.mess,"init");
    }

    args.GetReturnValue().Set(args.This());
    
  }

  void sbig::initialize_func(const FunctionCallbackInfo<Value>& args){
    Isolate* isolate = args.GetIsolate();
    
    const char* usage="usage: initialize( callback_function )";

    if (args.Length() != 1) {
      isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, usage)));
      return;
    }
    
    sbig* obj = ObjectWrap::Unwrap<sbig>(args.This());
    Local<Function> ccb=Local<Function>::Cast(args[0]);    
    
    send_status(isolate, ccb,"info","Initializing camera...","init");

    try{
      obj->initialize();
      send_status(isolate, ccb,"success","Camera is ready","init");
    }
    catch (qk::exception& e){
      send_status(isolate, ccb,"error",e.mess,"init");
    }
    args.GetReturnValue().Set(args.This());
  }



  
  //template <class T> 
  //Persistent<Function> 
  //jsmat<unsigned short>::constructor;


  void sbig::start_exposure_func(const FunctionCallbackInfo<Value>& args){
    Isolate* isolate = args.GetIsolate();

    sbig* obj = ObjectWrap::Unwrap<sbig>(args.This());
    //obj->cb = Local<Function>::Cast(args[0]);    
    Local<Function> cb=Local<Function>::Cast(args[0]);
    
    Handle<Value> fff=args.This()->Get(String::NewFromUtf8(isolate, "exptime"));
    obj->exptime = fff->ToNumber()->Value();

    fff=args.This()->Get(String::NewFromUtf8(isolate, "nexpo"));
    obj->nexpo = fff->ToNumber()->Value();

    send_status(isolate, cb,"info","Initializing exposure...","expo_proc");

    try{

      obj->start_exposure();
      send_status(isolate, cb,"info","Exposure started!","expo_proc");

      bool waiting=true;
      obj->event_id=0;


      while(waiting){
	obj->new_event.lock();

	while(obj->event_id==0){
	  obj->new_event.wait();
	  //expt.done.unlock();
	  
	  
	}
	
	if(obj->event_id==13) {
	  waiting=false;
	}

	if(obj->event_id==14) {
	  char nstr[64]; sprintf(nstr,"%g",obj->complete);
	  send_status(isolate, cb,"expo_progress",nstr,"expo_proc");
	}

	if(obj->event_id==15) {
	  char nstr[64]; sprintf(nstr,"%g",obj->complete);
	  send_status(isolate, cb,"grab_progress",nstr,"expo_proc");
	}

	if(obj->event_id==666) {
	  waiting=false;
	  send_status(isolate, cb,"error",obj->error_message,"expo_proc");
	}
	
	if(obj->event_id==11) {

	  /*
	  jsmat<unsigned short>* jsm=new jsmat<unsigned short>();
	  jsm->redim(obj->last_image.dims[0],obj->last_image.dims[1]);
	  for(int i=0;i<jsm->dim;i++)(*jsm)[i]=obj->last_image[i];
	  */

	  /*
	  int dims[2]={obj->ccd_width,obj->ccd_height};

	  size_t image_size=dims[0]*dims[1]*2;
	  char* image_data=(char*)obj->last_image.c;
	  Buffer* bp = Buffer::New(image_data, image_size, NULL, NULL); 
	  Handle<Buffer> hb(bp);
	  //obj->gen_pngtile(parameters);
	    //Buffer* bp =	  
	  
	  const unsigned argc = 1;
	  
	  v8::Handle<v8::Object> msg = v8::Object::New();
	  msg->Set(String::NewFromUtf8(isolate, "image"),hb->handle_);  
	  
	  v8::Handle<v8::Value> msgv(msg);
	  Handle<Value> argv[argc] = { msgv };
	  */

	  //jsmat<unsigned short>* jsmp;

	  //


	  //Handle<Object> jsmv = jsmat<unsigned short>::Instantiate();
	  
	  //cout << "JSM empty ? " << jsm.IsEmpty() << endl;
	  Local<Function> jsu_cons = Local<Function>::New(isolate, jsmat<unsigned short>::constructor);
	  
	  Handle<Value> jsm =jsu_cons->NewInstance();	  
	  Handle<Object> jsmo = Handle<Object>::Cast(jsm);
	  
	  //cout << "JSMO empty ? " << jsmo.IsEmpty() << endl;


	  if(!jsmo.IsEmpty()){

	    Handle<Value> fff=args.This()->Get(String::NewFromUtf8(isolate, "last_image"));


	    jsmat<unsigned short>* last_i = ObjectWrap::Unwrap<jsmat<unsigned short> >(Handle<Object>::Cast(fff));
	    jsmat<unsigned short>* jsmv_unw = ObjectWrap::Unwrap<jsmat<unsigned short> >(Handle<Object>::Cast(jsmo));
	    
	    //jsmat<unsigned short>* last_i = jsmat_unwrap<unsigned short>(Handle<Object>::Cast(fff)); //new jsmat<unsigned short>();
	    //jsmat<unsigned short>* jsmv_unw = jsmat_unwrap<unsigned short>(Handle<Object>::Cast(jsmo)); //new jsmat<unsigned short>();

	    //cout << "COPY "<< jsmv_unw << " LIMG w ="<<obj->last_image.dims[0]<< endl;
	    (*jsmv_unw)=obj->last_image;
	    (*last_i)=obj->last_image;
	    //cout << "COPY OK w="<< jsmv_unw->dims[0] << endl;

	    Handle<Value> h_fimage=args.This()->Get(String::NewFromUtf8(isolate, "last_image_float"));
	    jsmat<float>* fimage = ObjectWrap::Unwrap<jsmat<float> >(Handle<Object>::Cast(h_fimage)); //new jsmat<unsigned short>();
	    fimage->redim(last_i->dims[0],last_i->dims[1]);
	    for(int p=0;p<fimage->dim;p++)fimage->c[p]=(float)last_i->c[p];

	    
	    const unsigned argc = 1;
	    
	    v8::Handle<v8::Object> msg = v8::Object::New(isolate);
	    msg->Set(String::NewFromUtf8(isolate, "type"),String::NewFromUtf8(isolate, "new_image"));
	    msg->Set(String::NewFromUtf8(isolate, "content"),h_fimage);  
	    //if(id!="")
	    msg->Set(String::NewFromUtf8(isolate, "id"),String::NewFromUtf8(isolate, "expo_proc"));  
	    v8::Handle<v8::Value> msgv(msg);
	    Handle<Value> argv[argc] = { msgv };
	    cb->Call(isolate->GetCurrentContext()->Global(), argc, argv );    

	    /*
	    v8::Handle<v8::Object> msg = v8::Object::New();
	    msg->Set(String::NewFromUtf8(isolate, "new_image"),h_fimage);  
	    //v8::Handle<v8::Value> msgv(msg);
	    
	    Handle<Value> argv[1] = { msg };
	    obj->cb->Call(Context::GetCurrent()->Global(), 1, argv );    
	    */

	  }else{
	    cout << "BUG ! empty handle !"<<endl;
	  }
	  

	  //cout << " FieldCount = " << jsmo->InternalFieldCount() << endl;
	  //Handle<Object> jsmo = jsmat<unsigned short>::New();
	  //Handle<jsmat<unsigned short> > jsmh = Handle<jsmat<unsigned short> >::Cast(jsm);
	  //cout << "DIMS " << jsm->dims[0] << endl;
	  //Local<Value> jsmv = Local<Value>::New(jsm);
	  //Local<Object> jsmv = Local<Object>::New(jsmat<unsigned short>::Instantiate());
	  //(jsmat<unsigned short>*)(External::Unwrap(jsm));
	  //jsmat<unsigned short>* jsmv_unw = (External::Unwrap<jsmat<unsigned short> >(jsm));
	  //ObjectWrap::Unwrap<jsmat<unsigned short> >(*jsm);	  
	  //v8::Handle<v8::Object> new_ho = ObjectWrap::Wrap<jsmat<unsigned short> >(jsmv_unw);
	  //v8::Handle<v8::Value> msgv(jsmv);
	  //jsmv_unw->Wrap(jsm);
	  //Handle<Value> argv[1] = { jsmo };
	  //obj->cb->Call(Context::GetCurrent()->Global(), 1, argv );    
	  //obj->send_status_message("image","New event!");
	  
	}
	obj->event_id=0;
	obj->new_event.unlock();
      }

      send_status(isolate, cb,"success","Exposure done","expo_proc");
      
      
    }
    catch (qk::exception& e){
      send_status(isolate, cb,"error",e.mess,"expo_proc");
    }

    args.GetReturnValue().Set(args.This());

  }

  void sbig::stop_exposure_func(const FunctionCallbackInfo<Value>& args){

    Isolate* isolate = args.GetIsolate();

    sbig* obj = ObjectWrap::Unwrap<sbig>(args.This());


    obj->cb = Local<Function>::Cast(args[0]);
    obj->send_status_message(isolate, "info","stop exposure");
    obj->stop_exposure();
    
    args.GetReturnValue().Set(args.This());
  }

  void sbig::shutdown(){
    if(!pcam) return;

    stop_exposure();

    //void *x=0;
    MINFO << "Waiting for end of operations.." << endl;

    new_event.lock();
    while(expt.running){
      new_event.wait();

    }
    new_event.unlock();
    
    MINFO << "Closing cam devices.." << endl;

    if(pcam->CheckLink()){
      pcam->CloseDevice();
      check_error();
      pcam->CloseDriver();
      check_error();
    }
    
    MINFO << "Closing cam devices..done " << endl;

    delete pcam;
    pcam=0;
    MINFO << "pcam pointer deleted " << endl;
  }
  
  void sbig::check_error(){
    PAR_ERROR err;
    if((err = pcam->GetError()) != CE_NO_ERROR) 
      throw qk::exception("SBIG Error : "+pcam->GetErrorString(err));    

  }

  void sbig::initialize(){

    shutdown();

    MINFO << "Connecting to camera ..." << endl;

    pcam = new sbig_cam(this, DEV_USB1);
    
    check_error();
  
    string caminfo="";
    double ccd_temp;
    unsigned short regulation_enabled;
    double setpoint_temp;
    double percent_power;

    MINFO << "Connected to camera ..."<<endl;

    pcam->QueryTemperatureStatus(regulation_enabled, ccd_temp,setpoint_temp, percent_power);

    try{
      check_error();
    }
    catch(qk::exception& e){
      cerr << e.mess << endl;
    }

    // pcam->GetFormattedCameraInfo(caminfo, 0);      
      
    // try{
    //   check_error();
    // }
    // catch(qk::exception& e){
    //   cerr << e.mess << endl;
    // }
    
    pcam->GetFullFrame(ccd_width, ccd_height);

    MINFO << "Connected to camera : [" << pcam->GetCameraTypeString() << "] cam info = ["<< caminfo<<"]"<<endl;
    MINFO << "CCD Temperature regulation : " << (regulation_enabled? "ON" : "OFF")
	  << ", setpoint = " << setpoint_temp <<  " °C. Current CCD temperature =  " << ccd_temp << " °C."
	  << " Cooling power : "<< percent_power << "%."<<endl;
    
    
    //if(mode=="dark") sbdf=SBDF_DARK_ONLY;
    //else if(mode=="darklight") sbdf=SBDF_DARK_ALSO;
    

    // pcam->GetCCDTemperature(ccd_temp);
    // cout << "CCD Temperature: " << ccd_temp << endl;

    //    double exptime=5.0;


    // unsigned int readout_mode=2;                                                                                                                                         
    // pCam->SetReadoutMode(readout_mode);                                                                                                                                  
    // pImg->SetBinning(2,2);                                                                                                                                               

    // Set subframe:                                                                                                                                                        
    // pCam->SetSubFrame(nLeft, nTop, nWidth, nHeight);                                                                                                                     


    // if((err = pcam->EstablishLink()) != CE_NO_ERROR) 
    //   cout << "Error link !"<< endl;

    //    cout << "Link Established to Camera Type: " << pcam->GetCameraTypeString() << endl;

    // Subframe definition:                                                                                                                                                     
    // int                                                   nLeft   = 0;
    // int                                                           nTop    = 0;
    // int                                                           nWidth  = 0;
    // int                                                           nHeight = 0;


    // pcam->GetFullFrame( nWidth, nHeight);
    // cout << "Curent frame -> "<<nLeft<<","<< nTop<<","<<nWidth<<","<<nHeight<<endl;



    //if((err = pCam->GrabImage(pImg, SBDF_DARK_ALSO)) != CE_NO_ERROR)        break;                                                                                        
    // if((err = pcam->GrabImage(pImg, SBDF_LIGHT_ONLY)) != CE_NO_ERROR)
    //   cout << "Error grab !!" << endl;

    // pcam->SetExposureTime(2.0);
    // check_error();
    // //pcam->SetReadoutMode(2);
    // //pImg->SetBinning(2,2);      

    // MINFO << "Grabing image ...." << endl;
    // pcam->GrabImage(pImg,sbdf);
    // check_error();

    
  }

  void sbig::start_exposure(){

    continue_expo_mut.lock();
    bool already_exposing = continue_expo;
    continue_expo_mut.unlock();
    
    if(already_exposing) throw qk::exception("An exposure is already taking place !, Stop it first.");

    new_event.lock();

    expt.start();
    expt.running=1;

    new_event.unlock();

    /*
    last_image_ready_cond.lock();
    last_image_ready=0;
    last_image_ready_cond.unlock();
    */

  }

  void sbig::stop_exposure(){

    continue_expo_mut.lock();
    continue_expo=0;
    continue_expo_mut.unlock();

  }

  void sbig::really_take_exposure(){

    if(!pcam) throw qk::exception("No PCAM !");
    
    SBIG_DARK_FRAME sbdf=SBDF_LIGHT_ONLY;

    pcam->EstablishLink();
    check_error();
    
    CSBIGImg *pImg= 0;    
    pImg = new CSBIGImg;

    //MINFO << "Accumulating photons .... exptime="<<exptime << endl;

    //pCam->GetFullFrame( nWidth, nHeight);
    
    
    /*
    tracking* tr=dynamic_cast<tracking*>(parent);
    if(!tr){
      throw qk::exception("No track parent !");
    }
    */

    //    matrix<unsigned short>* trimg =new matrix<unsigned short>();  
    //tr->create_child(*trimg);
    
    int expo=0;

    //    continue_expo_mut.lock();
    continue_expo=1;
    //    continue_expo_mut.unlock();
    //    continue_expo_mut.lock();

    pcam->SetExposureTime(exptime);
    //      exptime.unuse();
    check_error();

    while(continue_expo){
      
      //      exptime.use();

      //MINFO << "Grabing image  "<< (expo+1) << "/" << nexpo<<endl;

      
      //void CSBIGCam::GetGrabState(GRAB_STATE &grabState, double &percentComplete)
      
      pcam->GrabImage(pImg,sbdf);
      check_error();
      

      new_event.lock();
      event_id=11;
      last_image.redim(pImg->m_nWidth, pImg->m_nHeight);    
      last_image.rawcopy(pImg->m_pImage,last_image.dim);
      
      //      MINFO << " OK. last image width is "<< last_image.dims[0] << endl;
      new_event.broadcast();
      new_event.unlock();


      /*
      sbig* obj=this;

	  int dims[2]={obj->ccd_width,obj->ccd_height};
	  size_t image_size=dims[0]*dims[1]*2;
	  char* image_data=(char*)obj->last_image.c;
	  Buffer* bp = Buffer::New(image_data, image_size, NULL, NULL); 
	  Handle<Buffer> hb(bp);
	  //obj->gen_pngtile(parameters);
	    //Buffer* bp =	  
	  
	  const unsigned argc = 1;
	  
	  v8::Handle<v8::Object> msg = v8::Object::New();
	  msg->Set(String::NewFromUtf8(isolate, "image"),hb->handle_);  
	  
	  v8::Handle<v8::Value> msgv(msg);
	  Handle<Value> argv[argc] = { msgv };
	  cb->Call(Context::GetCurrent()->Global(), argc, argv );    
      */



      
      //      last_image_ready_cond.lock();
      //      last_image.use();



      //   last_image.notifications.set_attribute(DataChanged);
      // last_image.change();
      // last_image.unuse();
      // 

      /*
      gl_matrix_view * glmv;

      //      tr->use();

      try{
	if(!tr->glv_tracking) throw qk::exception("No trackking !"); 
	glmv = dynamic_cast<gl_matrix_view*>(tr->glv_tracking->scenes.childs[0]);
	if(!glmv)
	  throw qk::exception("No GLMV !"); 


	MINFO << "We have the glmv ! " << glmv->get_info() << endl;

	// matrix<unsigned short>* trimg = dynamic_cast<matrix<unsigned short>*>(glmv->childs[0]);  
	
	// MINFO << "We have the trimg ! " << endl;
	// MINFO << "We have the trimg ! " << trimg->get_info() << endl;

	// //	*trimg=last_image;

	// MINFO << "We have the trimg updated ! " << endl;

	// vec<GLubyte> td;
	// //    td.set_all(0);
	// int tdi[2]={-1,-1};
	// MINFO << "Upate texture"<<endl;
	// glmv->create_image_texture();
	// MINFO << "Upate texture ok"<<endl;
	//glmv->update_texture_data(tdi, td);


      }
      
      catch(qk::exception& e){
	MERROR << "Error updateing tracker : "<< e.mess << endl;
      }




      tr->dispatcher();

      */

      //tr->unuse();
      
      expo++;
      
      if(!infinite_loop && expo>=nexpo) continue_expo=0;
      
      // //      continue_expo_mut.lock();
      // last_image_ready=1;
      // last_image_ready_cond.broadcast();
      // //last_image_ready_cond.unlock();


      // // last_image_ready_cond.lock();
      // last_image_ready=0;
      // last_image_ready_cond.unlock();

      /*
      
      time_t tim= time(NULL);
      //printf("Time = [%s]",tstring);
      char* ts=ctime(&tim);
      ts[strlen(ts)-1]=0;

      char fn[256];
      sprintf(fn,"fits_images/%s.fits",ts);
      
      SBIG_FILE_ERROR  ferr;
      MINFO << "Saving  image to " << fn        << endl;
      if((ferr = pImg->SaveImage(fn               , SBIF_FITS)) != SBFE_NO_ERROR) {
      	MERROR << "Error saving image !" << endl;
      }
      */

      //system("ds9 grab.fits -frame refresh");
      
    }
    
    continue_expo=0;
    //    continue_expo_mut.unlock();
    // MINFO << " Done exposures. Forcing closing of shutter." << endl;

    close_shutter();




  }


  void sbig::close_shutter(){

    MiscellaneousControlParams mcp;

    mcp.fanEnable=0;
    mcp.ledState=LED_BLINK_HIGH;
    mcp.shutterCommand=SC_CLOSE_SHUTTER;

    //    short int err;
    //err=
    SBIGUnivDrvCommand(CC_MISCELLANEOUS_CONTROL, &mcp, NULL);    
    check_error();
    
    mcp.fanEnable=0;
    mcp.ledState=LED_ON;
    mcp.shutterCommand=SC_CLOSE_SHUTTER;
    //err=
    SBIGUnivDrvCommand(CC_MISCELLANEOUS_CONTROL, &mcp, NULL);    
    check_error();

    //    MINFO  << " Shutter closed !" << endl;
  }


  bool sbig::expo_thread::exec(){
    //    MINFO << "Starting exposure...." << endl;

    try{
      sbc->really_take_exposure();
      sbc->new_event.lock();
      running=0;
      sbc->event_id=13;
      sbc->new_event.broadcast();
      sbc->new_event.unlock();

    }
    catch(qk::exception& e){
      MERROR << "Error in exposure thread :" << e.mess << endl;
      sbc->error_message=e.mess;
      sbc->new_event.lock();
      running=0;
      sbc->event_id=666;
      sbc->new_event.broadcast();
      sbc->new_event.unlock();
      
    }
    //MINFO << "End exposure..." << endl;

    // done.lock();
    
    // done.broadcast();
    // done.unlock();
    return true;
  }

  /*

  void sbig_ccd_temperature_control::command_func(){

    //    MINFO << "-------------TEMPERATURE--------------"    <<endl;

    MINFO << "Action is  [" << action << "]"<<endl;
   
    CSBIGCam *pcam = 0;
    PAR_ERROR err;
    pcam = new CSBIGCam(DEV_USB1);
    
    try{
      if((err = pcam->GetError()) != CE_NO_ERROR) throw 1;
      if((err = pcam->EstablishLink()) != CE_NO_ERROR) throw 1;

      MINFO << "Connected to camera : " << pcam->GetCameraTypeString() << endl;

      string stt;
      stt=action;
      
      MINFO << "Query temperature status..."<<endl;
      
      double ccd_temp;
      unsigned short regulation_enabled;
      double setpoint_temp;
      double percent_power;
      
      pcam->QueryTemperatureStatus(regulation_enabled, ccd_temp,setpoint_temp, percent_power);
      
      MINFO << "CCD Temperature regulation : " << (regulation_enabled? "ON" : "OFF")
	    << ", setpoint = " << setpoint_temp <<  " °C. Current CCD temperature =  " << ccd_temp << " °C."
	    << " Cooling power : "<< percent_power << "%."<<endl;
      
      if(stt=="status"){

	//	pcam->GetCCDTemperature(ccd_temp);
	
      }else
	if(action=="set"){
	  if(temp()!=-1){
	    pcam->SetTemperatureRegulation(1, temp());
	    if((err = pcam->GetError()) != CE_NO_ERROR) throw 1;
	    MINFO << "Temperature regulation is ON. Setpoint = " << temp() << " °C."<<endl;
	  }
	  else{
	    pcam->SetTemperatureRegulation(0, 0.0);
	    if((err = pcam->GetError()) != CE_NO_ERROR) throw 1;
	    MINFO << "Temp regulation is OFF."<<endl;
	  }
	}
      
      
    }

    catch (int e){
      MERROR << "Camera Error: " << pcam->GetErrorString(err) << endl;
      if((err = pcam->CloseDevice()) != CE_NO_ERROR){       MERROR << "Camera Error: " << pcam->GetErrorString(err) << endl;}
      if((err = pcam->CloseDriver()) != CE_NO_ERROR){       MERROR << "Camera Error: " << pcam->GetErrorString(err) << endl;}		
      delete pcam;
      throw 2;
    }

    if((err = pcam->CloseDevice()) != CE_NO_ERROR){       MERROR << "Camera Error: " << pcam->GetErrorString(err) << endl;}
    if((err = pcam->CloseDriver()) != CE_NO_ERROR){       MERROR << "Camera Error: " << pcam->GetErrorString(err) << endl;}		
    
    delete pcam;
    
  }
  
  void sbig_ccd_exposure::command_func(){


    CSBIGCam *pcam = 0;
    PAR_ERROR err;
    pcam = new CSBIGCam(DEV_USB1);
    
    SBIG_FILE_ERROR  ferr;
    CSBIGImg 			    *pImg	= 0;    

    MINFO << "-------------EXPOSURE--------------"<<endl<<endl;
    
    MINFO << "Mode is " << mode << endl;
    MINFO << "File_Name is " << file_name << endl;
    MINFO << "Readout_Mode is " << readout_mode() << endl;
    MINFO << "Exptime is " << exptime() << endl;
    

    pImg = new CSBIGImg;
      
    try{    

      if((err = pcam->GetError()) != CE_NO_ERROR) throw 1;
      if((err = pcam->EstablishLink()) != CE_NO_ERROR) throw 1;
      MINFO << "Connected to camera : " << pcam->GetCameraTypeString() << endl;

      if(action=="grab"){

	SBIG_DARK_FRAME sbdf=SBDF_LIGHT_ONLY;
	
	if(mode=="dark") sbdf=SBDF_DARK_ONLY;
	else if(mode=="darklight") sbdf=SBDF_DARK_ALSO;

	
	pcam->SetExposureTime(exptime());
	//pcam->SetReadoutMode(readout_mode());
	//pImg->SetBinning(2,2);      

	MINFO << "Taking light...." << endl;

	if((err = pcam->GrabImage(pImg,sbdf)) != CE_NO_ERROR)  throw 1;
	
	MINFO << "Saving  image to " << file_name << endl;
	if((ferr = pImg->SaveImage(file_name.c_str(), SBIF_FITS)) != SBFE_NO_ERROR) {
	  MERROR << "Error saving image !" << endl;
	}
      }

    }

    catch (int e){
      MERROR << "Camera Error: " << pcam->GetErrorString(err) << endl;
      if((err = pcam->CloseDevice()) != CE_NO_ERROR){       MERROR << "Camera close device: " << pcam->GetErrorString(err) << endl;}
      if((err = pcam->CloseDriver()) != CE_NO_ERROR){       MERROR << "Camera close driver: " << pcam->GetErrorString(err) << endl;}		      
      delete pcam;
      delete pImg;
      throw 2;
    }

    
    if((err = pcam->CloseDevice()) != CE_NO_ERROR){       MERROR << "Camera Error: " << pcam->GetErrorString(err) << endl;}
    if((err = pcam->CloseDriver()) != CE_NO_ERROR){       MERROR << "Camera Error: " << pcam->GetErrorString(err) << endl;}		
      
    delete pcam;
    delete pImg;
    }
  */

  


  //  template <class T> Persistent<FunctionTemplate>  jsmat<T>::s_ctm;
  // template <class T> Persistent<Function> jsmat<T>::constructor;

  template class jsmat<unsigned short> ;
  //Persistent<FunctionTemplate> jsmat<unsigned short>::s_ctm;

  void init_node_module(Handle<Object> exports) {

    //colormap_interface::init(exports);
    cout << "Init sbig c++ plugin..." << endl;
    sbig::init(exports);
    cout << "Init sbig done "<<endl;
    jsmat<unsigned short>::init(exports,"mat_ushort");
    jsmat<float>::init(exports,"mat_float");

  }
  
  NODE_MODULE(sbig, init_node_module)
}


