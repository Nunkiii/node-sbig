/*
  The Quarklib project.
  Written by Pierre Sprimont, <sprimont@email.ru>, INAF/IASF-Bologna, 2014-2019.
  Do good things with this code!
*/


/*#include "tracking.hh"*/

#include <thread>
#include <algorithm>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <iostream>
#include <queue>
#include <chrono>
#include <map>

#include <time.h>
#include "sbig.hh"

#include "qk/pngwriter.hh"
#include "math/jsmat_nan.hh"
// #include "node_modules/node-fits/qk/pngwriter.hh"
// #include "node_modules/node-fits/math/jsmat_nan.hh"
// #include "../node_modules/node-fits/qk/pngwriter.hh"
// #include "../node_modules/node-fits/math/jsmat_nan.hh"

namespace sadira{

  //  using namespace v8;

  using namespace Nan;
  using namespace std;  
  using namespace qk;
  

  std::map<int,std::string> cam_events;
  std::map<int,std::string> cam_commands;

#define EVT_ERROR 0
#define EVT_INIT_REPORT 1
#define EVT_GRAB_PROGRESS 2
#define EVT_EXPO_PROGRESS 3
#define EVT_EXPO_COMPLETE 4
#define EVT_COOLING_REPORT 5
#define EVT_NEW_IMAGE 6
#define EVT_SHUTTER 7
  

  
#define COM_INITIALIZE 0
#define COM_SHUTDOWN   1
#define COM_EXPO 2
#define COM_SHUTTER 3
#define COM_MONITOR 4  
  
  void setup_cam_events(){

    cam_events.insert(std::make_pair(EVT_ERROR,"error"));
    cam_events.insert(std::make_pair(EVT_INIT_REPORT,"init_report"));
    cam_events.insert(std::make_pair(EVT_GRAB_PROGRESS,"grab_progress"));
    cam_events.insert(std::make_pair(EVT_EXPO_PROGRESS,"expo_progress"));
    cam_events.insert(std::make_pair(EVT_EXPO_COMPLETE,"expo_complete"));
    cam_events.insert(std::make_pair(EVT_COOLING_REPORT,"cooling_report"));
    cam_events.insert(std::make_pair(EVT_NEW_IMAGE,"new_image"));
    cam_events.insert(std::make_pair(EVT_SHUTTER,"shutter"));


    cam_commands.insert(std::make_pair(COM_INITIALIZE,"initialize"));
    cam_commands.insert(std::make_pair(COM_SHUTDOWN, "shutdown"));
    cam_commands.insert(std::make_pair(COM_EXPO, "exposure"));
    cam_commands.insert(std::make_pair(COM_SHUTTER, "shutter"));
    cam_commands.insert(std::make_pair(COM_MONITOR, "monitor"));
    
  }
  
  //uv_async_t async;
  
  void sbig_cam::check_error(){
    PAR_ERROR err;
    if((err = this->GetError()) != CE_NO_ERROR) 
      throw qk::exception("SBIG Error : "+this->GetErrorString(err));    
    
  }

  PAR_ERROR sbig_cam::GrabSetup()//CSBIGImg *pImg, SBIG_DARK_FRAME dark)
  {
    GetCCDInfoParams 	gcip;
    GetCCDInfoResults0 	gcir;
    unsigned short 		es;
    string 				s;
    
    // Get the image dimensions
    m_eGrabState   = GS_DAWN;
	m_dGrabPercent = 0.0;
	m_sGrabInfo.vertNBinning = m_uReadoutMode >> 8;

	if (m_sGrabInfo.vertNBinning == 0)
	{
		m_sGrabInfo.vertNBinning = 1;
	}
	m_sGrabInfo.rm   = m_uReadoutMode & 0xFF;
	m_sGrabInfo.hBin = m_sGrabInfo.vBin = 1;

	if (m_eCameraType == STI_CAMERA)
	{
		if (m_sGrabInfo.rm < 2)
		{
			m_sGrabInfo.hBin = m_sGrabInfo.vBin = (m_sGrabInfo.rm + 1);
		}
		else if (m_sGrabInfo.rm < 4)
		{
			m_sGrabInfo.hBin = (m_sGrabInfo.rm - 1);
			m_sGrabInfo.vBin = m_sGrabInfo.vertNBinning;
		}
	}
	else
	{
		if (m_sGrabInfo.rm < 3)
		{
			m_sGrabInfo.hBin = m_sGrabInfo.vBin = (m_sGrabInfo.rm + 1);
		}
		else if (m_sGrabInfo.rm < 6)
		{
			m_sGrabInfo.hBin = (m_sGrabInfo.rm - 5);
			m_sGrabInfo.vBin = m_sGrabInfo.vertNBinning;
		}
		else if (m_sGrabInfo.rm < 9)
		{
			m_sGrabInfo.hBin = m_sGrabInfo.vBin = (m_sGrabInfo.rm - 8);
		}
		else if (m_sGrabInfo.rm == 9)
		{
			m_sGrabInfo.hBin = m_sGrabInfo.vBin = 9;
		}
	}
	gcip.request = (m_eActiveCCD == CCD_IMAGING ? CCD_INFO_IMAGING : CCD_INFO_TRACKING);

	if (SBIGUnivDrvCommand(CC_GET_CCD_INFO, &gcip, &gcir) != CE_NO_ERROR)
	{
		return m_eLastError;
	}

	if (m_sGrabInfo.rm >= gcir.readoutModes)
	{
		return CE_BAD_PARAMETER;
	}

	if (m_nSubFrameWidth == 0 || m_nSubFrameHeight == 0)
	{
		m_sGrabInfo.left  = m_sGrabInfo.top = 0;
		m_sGrabInfo.width = gcir.readoutInfo[m_sGrabInfo.rm].width;

		if (m_eCameraType == STI_CAMERA)
		{
			if (m_sGrabInfo.rm >= 2 && m_sGrabInfo.rm <= 3)
			{
				m_sGrabInfo.height = gcir.readoutInfo[m_sGrabInfo.rm-2].height / m_sGrabInfo.vertNBinning;
			}
			else
			{
				m_sGrabInfo.height = gcir.readoutInfo[m_sGrabInfo.rm].height / m_sGrabInfo.vertNBinning;
			}
		}
		else
		{
			if (m_sGrabInfo.rm >= 3 && m_sGrabInfo.rm <= 5)
			{
				m_sGrabInfo.height = gcir.readoutInfo[m_sGrabInfo.rm-3].height / m_sGrabInfo.vertNBinning;
			}
			else
			{
				m_sGrabInfo.height = gcir.readoutInfo[m_sGrabInfo.rm].height / m_sGrabInfo.vertNBinning;
			}
		}
	}
	else
	{
		m_sGrabInfo.left 	= m_nSubFrameLeft;
		m_sGrabInfo.top 	= m_nSubFrameTop;
		m_sGrabInfo.width 	= m_nSubFrameWidth;
		m_sGrabInfo.height 	= m_nSubFrameHeight;
	}

	// try to allocate the image buffer
	// if (!pImg->AllocateImageBuffer(m_sGrabInfo.height, m_sGrabInfo.width))
	// {
	// 	m_eGrabState = GS_IDLE;
	// 	return CE_MEMORY_ERROR;
	// }
	// pImg->SetImageModified(TRUE);

	// // initialize some image header params
	// pImg->SetEachExposure(m_dExposureTime);
	// pImg->SetEGain(hex2double(gcir.readoutInfo[m_sGrabInfo.rm].gain));
	// pImg->SetPixelHeight(hex2double(gcir.readoutInfo[m_sGrabInfo.rm].pixelHeight) * m_sGrabInfo.vertNBinning / 1000.0);
	// pImg->SetPixelWidth(hex2double(gcir.readoutInfo[m_sGrabInfo.rm].pixelWidth) / 1000.0);
	es = ES_DCS_ENABLED | ES_DCR_DISABLED | ES_AUTOBIAS_ENABLED;

	if (m_eCameraType == ST5C_CAMERA)
	{
		es |= (ES_ABG_CLOCKED | ES_ABG_RATE_MED);
	}
	else if (m_eCameraType == ST237_CAMERA)
	{
		es |= (ES_ABG_CLOCKED | ES_ABG_RATE_FIXED);
	}
	else if (m_eActiveCCD == CCD_TRACKING)
	{
		es |= (ES_ABG_CLOCKED | ES_ABG_RATE_MED);
	}
	else
	{
		es |= ES_ABG_LOW;
	}
	// pImg->SetExposureState(es);
	// pImg->SetExposureTime(m_dExposureTime);
	// pImg->SetNumberExposures(1);
	// pImg->SetReadoutMode(m_uReadoutMode);

	s = GetCameraTypeString();
	if (m_eCameraType == ST5C_CAMERA || ( m_eCameraType == ST237_CAMERA && s.find("ST-237A", 0) == string::npos))
	{
	  //pImg->SetSaturationLevel(4095);
	}
	else
	{
	  //pImg->SetSaturationLevel(65535);
	}
	s = gcir.name;
	MINFO << "cam model " << gcir.name << endl;
	// pImg->SetCameraModel(s);
	// pImg->SetBinning(m_sGrabInfo.hBin, m_sGrabInfo.vBin);
	// pImg->SetSubFrame(m_sGrabInfo.left, m_sGrabInfo.top);
	return CE_NO_ERROR;
}



  PAR_ERROR sbig_cam::GrabMainFast(qk::mat<unsigned short>& data){
    int 								i;
    //double 							ccdTemp = 0.0;
    //time_t 							curTime;
    PAR_ERROR 					err;
    StartReadoutParams 	srp;
    ReadoutLineParams 	rlp;
    //struct tm *					pLT;
    //char 								cs[80];
    MY_LOGICAL 					expComp;
    
    EndExposure();


    
    if (m_eLastError != CE_NO_ERROR && m_eLastError != CE_NO_EXPOSURE_IN_PROGRESS)
	{
		return m_eLastError;
	}
	
	// Record the image size incase this is an STX and its needs
	// the info to start the exposure
	SetSubFrame(m_sGrabInfo.left, m_sGrabInfo.top, m_sGrabInfo.width, m_sGrabInfo.height);

	// start the exposure
	m_eGrabState = GS_EXPOSING_LIGHT;
	
	if (StartExposure(SC_OPEN_SHUTTER) != CE_NO_ERROR)
	{
		return m_eLastError;
	}
	
	//cout << "EXPO BEGIN" << endl;
	do 
	{
	  //gpct= (double)(time(NULL) - curTime)/m_dExposureTime;
	  //cout << "EXPO% " << gpct*100 << " exptime  " << m_dExposureTime << endl;

	  //  if(gpct != m_dGrabPercent){
	  //m_dGrabPercent =gpct;
	    // }

	  usleep(800);
	} 
	while ((err = IsExposureComplete(expComp)) == CE_NO_ERROR && !expComp );

	//cout << "EXPO DONE" << endl;
	
	EndExposure();


	m_dGrabPercent = 0.0;
	
	if (err != CE_NO_ERROR)
	{
		return err;
	}
	
	if (m_eLastError != CE_NO_ERROR)
	{
		return m_eLastError;
	}
	
	// readout the CCD
	srp.ccd    = m_eActiveCCD;
	srp.left   = m_sGrabInfo.left;
	srp.top    = m_sGrabInfo.top;
	srp.height = m_sGrabInfo.height;
	srp.width  = m_sGrabInfo.width;
	srp.readoutMode = m_uReadoutMode;

	data.redim(m_sGrabInfo.width,m_sGrabInfo.height);m_eGrabState = GS_DIGITIZING_LIGHT;

	//int nnotif=5;
	if ( (err = StartReadout(srp)) == CE_NO_ERROR ) 
	{
		rlp.ccd = m_eActiveCCD;
		rlp.pixelStart = m_sGrabInfo.left;
		rlp.pixelLength = m_sGrabInfo.width;
		rlp.readoutMode = m_uReadoutMode;
	
		for (i = 0; i < m_sGrabInfo.height && err == CE_NO_ERROR; i++)
		{
		  //m_dGrabPercent = (double)(i+1) / m_sGrabInfo.height;
			err = ReadoutLine(rlp, FALSE, data.c + (long)i * m_sGrabInfo.width);
			//cout << "Grab" << m_dGrabPercent << endl;

			//		if(i%(int)(m_sGrabInfo.height*1.0/nnotif)==0)
			//grab_complete(m_dGrabPercent);
		}
		//		grab_complete(1.0);
	}
	
	EndReadout();

	cout << "Readout DONE !" << endl;
	
	if (err != CE_NO_ERROR)
	{
		return err;
	}
	
	if (m_eLastError != CE_NO_ERROR)
	{
		return err;
	}
	
 	cout << "DONE GRAB!" << endl;
		
	return CE_NO_ERROR;	
}


  
  void send_status_cb(Nan::Callback& cb, const string& type, const string& message, const string& id=""){
    
    const unsigned argc = 1;

    v8::Handle<v8::Object> msg = Nan::New<v8::Object>();//.ToLocalChecked(); 
    msg->Set(Nan::New<v8::String>("type").ToLocalChecked(),Nan::New<v8::String>(type.c_str()).ToLocalChecked());
    msg->Set(Nan::New<v8::String>("content").ToLocalChecked(),Nan::New<v8::String>(message.c_str()).ToLocalChecked());  
    if(id!="")
      msg->Set(Nan::New<v8::String>("id").ToLocalChecked(),Nan::New<v8::String>(id.c_str()).ToLocalChecked());  
    v8::Handle<v8::Value> msgv(msg);
    v8::Handle<v8::Value> argv[argc] = { msgv };

    cb.Call(argc, argv);    
  }

  void send_status_func(v8::Local<v8::Function>& cb, const string& type, const string& message, const string& id=""){
    
    const unsigned argc = 1;

    v8::Handle<v8::Object> msg = Nan::New<v8::Object>();//.ToLocalChecked(); 
    msg->Set(Nan::New<v8::String>("type").ToLocalChecked(),Nan::New<v8::String>(type.c_str()).ToLocalChecked());
    msg->Set(Nan::New<v8::String>("content").ToLocalChecked(),Nan::New<v8::String>(message.c_str()).ToLocalChecked());  
    if(id!="")
      msg->Set(Nan::New<v8::String>("id").ToLocalChecked(),Nan::New<v8::String>(id.c_str()).ToLocalChecked());  
    v8::Handle<v8::Value> msgv(msg);
    v8::Handle<v8::Value> argv[argc] = { msgv };

    v8::Isolate *isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    cb->Call(context->Global(), argc, argv );
    //    cb.Call(argc, argv );    
  }

  //sbig_driver class implementation.
  //A single object of this class must be instanciated. 


  //Nan::Persistent<Function> sbig_driver::constructor;
  
  sbig_driver::sbig_driver(){
  }
  sbig_driver::~sbig_driver(){
    MINFO << "Hello Destructor!" << endl; //Not called. No-GC?
  }
  
  // void sbig_driver::Destructor(napi_env env, void* nativev8::Object, void* /*finalize_hint*/) {
  //   reinterpret_cast<sbig_driver*>(nativev8::Object)->~sbig_driver();
  // }

  void sbig_driver::New(const Nan::FunctionCallbackInfo<v8::Value>& args){
    
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    
    if (args.IsConstructCall()) {
      
      //HandleScope scope;
      
      sbig_driver* obj = new sbig_driver();

      obj->Wrap(args.This());
      
      args.GetReturnValue().Set(args.This());
      //return args.This();
      
    }else{
      //const int argc = 1;
      //v8::Local<v8::Value> argv[argc] = { args[0] };
      
      v8::Local<v8::Function> cons = Nan::New(constructor());
      v8::Local<v8::Object> result =cons->NewInstance(context).ToLocalChecked();
      args.GetReturnValue().Set(result);
      
    }

  }
  
  void sbig_driver::init(v8::Local<v8::Object> target){
    
    v8::Isolate* isolate=target->GetIsolate();
    v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
    
    tpl->SetClassName(v8::String::NewFromUtf8(isolate, "sbig"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    
    // Prototype
    
    SetPrototypeMethod(tpl, "initialize_camera", initialize_camera_func); 
    
    target->Set(v8::String::NewFromUtf8(isolate,"driver"), tpl->GetFunction());

    constructor().Reset(GetFunction(tpl).ToLocalChecked());
    //constructor.Reset(isolate, tpl->GetFunction());
    
  }

  void sbig_driver::initialize_camera_func(const Nan::FunctionCallbackInfo<v8::Value>& args){
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();  

    const char* usage="usage: initialize_camera( DeviceID,  callback_function )";
    
    if (args.Length() != 2) {
      isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, usage)));
      return;
    }
    
//    sbig_driver* obj = v8::ObjectWrap::Unwrap<sbig_driver>(args.This());

    v8::Local<v8::Number> usb_id=v8::Local<v8::Number>::Cast(args[0]);    
//    v8::Local<Function> ccb=v8::Local<Function>::Cast(args[1]);    

    //MINFO << "Shutting dowm camera ... ID " << usb_id << endl;
    
    //    shutdown();

    MINFO << "Connecting to camera with USB ID = " << usb_id->Value() << endl;

    SBIG_DEVICE_TYPE dev= (SBIG_DEVICE_TYPE) (DEV_USB+2+usb_id->Value());
    //pcam = new sbig_cam(this, DEV_USB1);

    sbig_cam* pcam=NULL;

    try{
      
      pcam = new sbig_cam(NULL , dev);
    
      pcam->check_error();

      v8::Local<v8::Function> cam_cons = v8::Local<v8::Function>::New(isolate, sbig::constructor());
      v8::Local<v8::Object> camera =cam_cons->NewInstance(context).ToLocalChecked();
      
      args.GetReturnValue().Set(camera);
      
    }

    catch(qk::exception& e){
      
    }
    
  }
  
  
  //sbig class implementation.
  //An sbig object represents a single camera connected to the driver.
  
  //  Nan::Persistent<Function> sbig::constructor;


  sbig_cam::sbig_cam(sbig* _sbig, SBIG_DEVICE_TYPE dev): CSBIGCam(dev){
    

    sb=_sbig;

  }

  sbig_cam::sbig_cam(sbig* _sbig){
    sb=_sbig;
  }
  
  void sbig_cam::expo_complete(double pc){
    

    //    if(pc- sb->edata.complete < .01) return;

    //   MINFO << "Expo complete PC= " << pc << " SB->C = " << sb->edata.complete << "DIFF=" << pc- sb->edata.complete << endl;
    
    cam_event* came2=new cam_event();
    came2->obj=sb;
    came2->event=EVT_EXPO_PROGRESS;
    came2->complete=pc;
    sb->event_queue.push(came2);
    uv_async_send(&sb->AW.async);


  }


  void sbig_cam::grab_complete(double pc){

    //if(pc- sb->edata.complete < .01) return;

    cam_event* came2=new cam_event();
    came2->obj=sb;
    came2->event=EVT_GRAB_PROGRESS;
    came2->complete=pc;
    sb->event_queue.push(came2);
    uv_async_send(&sb->AW.async);
    

  }
  
  
  void sbig::New(const Nan::FunctionCallbackInfo<v8::Value>& args){

    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    
    if (args.IsConstructCall()) {
      
      //HandleScope scope;
      
      sbig* obj = new sbig();
      //  obj->counter_ = args[0]->IsUndefined() ? 0 : args[0]->NumberValue();
      //if(!args[0]->IsUndefined()){
      //v8::String::Utf8v8::Value s(args[0]->ToString());
      //obj->file_name=*s;
      //}

      //->Set(String::NewFromUtf8(isolate,"file_name"), args[0]);
      args.This()->Set(v8::String::NewFromUtf8(isolate,"exptime"), v8::Number::New(isolate, 0.5));
      args.This()->Set(v8::String::NewFromUtf8(isolate,"nexpo"), v8::Number::New(isolate, 5));
      
      v8::Local<v8::Function> jsu_cons = v8::Local<v8::Function>::New(isolate, jsmat<unsigned short>::constructor());
      v8::Local<v8::Function> jsf_cons = v8::Local<v8::Function>::New(isolate, jsmat<float>::constructor());

      v8::Local<v8::Object> last_image =jsu_cons->NewInstance(context).ToLocalChecked();
      v8::Local<v8::Object> last_image_float =jsf_cons->NewInstance(context).ToLocalChecked();
      
      args.This()->Set(v8::String::NewFromUtf8(isolate,"last_image"), last_image);
      args.This()->Set(v8::String::NewFromUtf8(isolate,"last_image_float"), last_image_float);
      obj->Wrap(args.This());
      
      
      
      args.GetReturnValue().Set(args.This());
      //return args.This();
      
    }else{
      const int argc = 1;
      v8::Local<v8::Value> argv[argc] = { args[0] };
      
      v8::Local<v8::Function> cons = v8::Local<v8::Function>::New(isolate, constructor());
      v8::Local<v8::Object> result =cons->NewInstance(context,argc,argv).ToLocalChecked();
      args.GetReturnValue().Set(result);

    }

  }
  

  //Nan::Persistent<FunctionTemplate> sbig::s_cts;

  void sbig::ccd_info_func(const Nan::FunctionCallbackInfo<v8::Value>& args){
    
    v8::Isolate* isolate = args.GetIsolate();
    sbig* obj = Nan::ObjectWrap::Unwrap<sbig>(args.This());
    sbig_cam* cam=obj->pcam;
    
    if(!cam){
      isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Camera not connected!")));
      return;
    }
    
    GetCCDInfoParams	par;
    GetCCDInfoResults0  res;

    double pixelWidth=0, pixelHeight=0, eGain=0;
    //cam->GetReadoutInfo(pixelWidth,pixelHeight,eGain);
    
    par.request = 1;
    obj->pcam->SBIGUnivDrvCommand(CC_GET_CCD_INFO, &par, &res);
    cam->check_error();
	
    MINFO << "OK CCD INFO! " << res.readoutModes <<endl;
	
    //res.readoutModes=10;
	
    v8::Handle<v8::Object> ccd_info = v8::Object::New(isolate);
    ccd_info->Set(v8::String::NewFromUtf8(isolate, "Readout modes"),v8::Number::New(isolate, res.readoutModes ));
    ccd_info->Set(v8::String::NewFromUtf8(isolate, "name"),v8::String::NewFromUtf8(isolate, res.name ));
    ccd_info->Set(v8::String::NewFromUtf8(isolate, "Gain"),v8::Number::New(isolate, eGain ));
    ccd_info->Set(v8::String::NewFromUtf8(isolate, "PixelWidth"),v8::Number::New(isolate, pixelWidth ));
    ccd_info->Set(v8::String::NewFromUtf8(isolate, "PixelHeight"),v8::Number::New(isolate, pixelHeight ));
      
    v8::Handle<v8::Array> ccd_modes = v8::Array::New(isolate);

    ccd_info->Set(v8::String::NewFromUtf8(isolate, "Readout information"),ccd_modes);

	
    for(int i=0;i<res.readoutModes;i++){
      v8::Handle<v8::Object> mode_info = v8::Object::New(isolate);

      //MINFO << "OK CCD INFO!" <<res.name <<  " ID " << i<< endl;

      mode_info->Set(v8::String::NewFromUtf8(isolate, "Mode"),v8::Number::New(isolate, res.readoutInfo[i].mode ));
      mode_info->Set(v8::String::NewFromUtf8(isolate, "Width"),v8::Number::New(isolate, res.readoutInfo[i].width ));
      mode_info->Set(v8::String::NewFromUtf8(isolate, "Height"),v8::Number::New(isolate, res.readoutInfo[i].height ));
      mode_info->Set(v8::String::NewFromUtf8(isolate, "Gain"),v8::Number::New(isolate, res.readoutInfo[i].gain ));
      mode_info->Set(v8::String::NewFromUtf8(isolate, "PixelWidth"),v8::Number::New(isolate, res.readoutInfo[i].pixelWidth ));
      mode_info->Set(v8::String::NewFromUtf8(isolate, "PixelHeight"),v8::Number::New(isolate, res.readoutInfo[i].pixelHeight ));
      //MINFO << "DONE CCD INFO!" <<res.name << endl;

      ccd_modes->Set(i,mode_info);
    }

    args.GetReturnValue().Set(ccd_info);
  }
  
  
  void sbig::set_temp_func(const Nan::FunctionCallbackInfo<v8::Value>& args){

    v8::Isolate* isolate = args.GetIsolate();
    
    if (args.Length() != 2) {
      isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Need setpoint info! 2 pars: (enabled, setpoint)")));
      return;
    }

    sbig* obj = Nan::ObjectWrap::Unwrap<sbig>(args.This());
    sbig_cam* cam=obj->pcam;
    
    if(!cam){
      isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Camera not connected!")));
      return;
    }

    short unsigned int cooling_enabled=args[0]->NumberValue();
    double setpoint=args[1]->NumberValue();
    PAR_ERROR res = CE_NO_ERROR;

    if ( (res = cam->SetTemperatureRegulation(cooling_enabled, setpoint) ) != CE_NO_ERROR ){
      isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Error setting CCD cooling!")));
      return;
    }
    
    args.GetReturnValue().Set(args.This());
  }
  
  
  void sbig::get_temp_func(const Nan::FunctionCallbackInfo<v8::Value>& args){

    v8::Isolate* isolate = args.GetIsolate();
    
    sbig* obj = Nan::ObjectWrap::Unwrap<sbig>(args.This());
    
    sbig_cam* cam=obj->pcam;

    if(!cam){
      isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Camera not connected!")));
      return;
    }


    

    PAR_ERROR res = CE_NO_ERROR;
    double d,setpoint,cooling_power;
    // CCD Temperature
    short unsigned int cooling_enabled;
    

    if ( (res = cam->QueryTemperatureStatus(cooling_enabled, d, setpoint, cooling_power)) != CE_NO_ERROR ){
      isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Error getting CCD temperature!")));
      return;
    }

    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    
    result->Set(v8::String::NewFromUtf8(isolate, "cooling"),v8::Number::New(isolate, cooling_enabled));
    result->Set(v8::String::NewFromUtf8(isolate, "cooling_setpoint"),v8::Number::New(isolate, setpoint));
    result->Set(v8::String::NewFromUtf8(isolate, "cooling_power"),v8::Number::New(isolate, cooling_power));
    result->Set(v8::String::NewFromUtf8(isolate, "ccd_temp"),v8::Number::New(isolate, d));
    
    QueryTemperatureStatusResults qtsr;
    
    // Ambient Temperature
    if ( (res = cam->SBIGUnivDrvCommand(CC_QUERY_TEMPERATURE_STATUS, NULL, &qtsr)) != CE_NO_ERROR ){
      MWARN << "Error getting Ambient temperature!"<<endl;
      //isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, "Error getting Ambient temperature!")));
    }else{

      d=cam->ADToDegreesC(qtsr.ambientThermistor, FALSE);
      result->Set(v8::String::NewFromUtf8(isolate, "ambient_temp"),v8::Number::New(isolate, d));
    }
    
    args.GetReturnValue().Set(result);

  }

  void sbig::init(v8::Local<v8::Object> target){
    
    v8::Isolate* isolate=target->GetIsolate();
    v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
    
    tpl->SetClassName(v8::String::NewFromUtf8(isolate, "sbig"));
    tpl->InstanceTemplate()->SetInternalFieldCount(9);

    // Prototype

    //    SetPrototypeMethod(tpl, "usb_info", usb_info_func); 
    SetPrototypeMethod(tpl, "initialize", initialize_func); 
    SetPrototypeMethod(tpl, "shutdown", shutdown_func);
    SetPrototypeMethod(tpl, "start_exposure",start_exposure_func);
    SetPrototypeMethod(tpl, "stop_exposure", stop_exposure_func);
    SetPrototypeMethod(tpl, "get_temp", get_temp_func);
    SetPrototypeMethod(tpl, "set_temp", set_temp_func);
    SetPrototypeMethod(tpl, "filter_wheel", filter_wheel_func);
    SetPrototypeMethod(tpl, "ccd_info", ccd_info_func);
    SetPrototypeMethod(tpl, "monitor", monitor_func);
    //SetPrototypeMethod(tpl, "sub_frame", sub_frame_func);


    target->Set(v8::String::NewFromUtf8(isolate,"cam"), tpl->GetFunction());

    //constructor.Reset(isolate, tpl->GetFunction());
    constructor().Reset(GetFunction(tpl).ToLocalChecked());    
  }

  

  
  void sbig::send_status_message(v8::Isolate* isolate, const string& type, const string& message){
MERROR << "deprecated" << endl;
//const unsigned argc = 1;

    v8::Handle<v8::Object> msg = v8::Object::New(isolate);
    msg->Set(v8::String::NewFromUtf8(isolate, "type"),v8::String::NewFromUtf8(isolate, type.c_str()));
    msg->Set(v8::String::NewFromUtf8(isolate, "content"),v8::String::NewFromUtf8(isolate, message.c_str()));  

    v8::Handle<v8::Value> msgv(msg);
    // v8::Handle<v8::Value> argv[argc] = { msgv };
    //    cb->Call(isolate->GetCurrentContext()->Global(), argc, argv );    
  }


  PAR_ERROR cfwInit(unsigned short cfwModel, CFWResults* pRes){
    PAR_ERROR	err;
    CFWParams cfwp;
    
    cfwp.cfwModel   = cfwModel;
    cfwp.cfwCommand = CFWC_INIT;
    err = (PAR_ERROR)SBIGUnivDrvCommand(CC_CFW, &cfwp, pRes);
    
    if (err != CE_NO_ERROR)
      {
	return err;
      }

    do
      {
	cfwp.cfwCommand = CFWC_QUERY;
	err = (PAR_ERROR)SBIGUnivDrvCommand(CC_CFW, &cfwp, pRes);
	
	if (err != CE_NO_ERROR)
	  {
	    continue;
	  }
	
	if (pRes->cfwStatus != CFWS_IDLE)
	  {
	    sleep(1);
	  }
      }
    while (pRes->cfwStatus != CFWS_IDLE);
    
    return err;
  }
  
  //==============================================================
  PAR_ERROR cfwGoto(unsigned short cfwModel, int cfwPosition, CFWResults* pRes){
    PAR_ERROR err;
    CFWParams cfwp;
    
    cfwp.cfwModel = cfwModel;

    do
      {
	cfwp.cfwCommand = CFWC_QUERY;
	err = (PAR_ERROR)SBIGUnivDrvCommand(CC_CFW, &cfwp, pRes);
	
	if (err != CE_NO_ERROR)
	  {
	    continue;
	  }
	
	if (pRes->cfwStatus != CFWS_IDLE)
	  {
	    sleep(1);
	  }
      }
    while (pRes->cfwStatus != CFWS_IDLE);
    
    cfwp.cfwCommand = CFWC_GOTO;
    cfwp.cfwParam1  = cfwPosition;
    err = (PAR_ERROR)SBIGUnivDrvCommand(CC_CFW, &cfwp, pRes);
    if (err != CE_NO_ERROR)
      {
	return err;
      }
    
    do
      {
	cfwp.cfwCommand = CFWC_QUERY;
	err = (PAR_ERROR)SBIGUnivDrvCommand(CC_CFW, &cfwp, pRes);
	
	if (err != CE_NO_ERROR)
	  {
	    continue;
	  }
	
	if (pRes->cfwStatus != CFWS_IDLE)
	  {
	    sleep(1);
	  }
      }
    while (pRes->cfwStatus != CFWS_IDLE);
    
    return err;
  }
  //==============================================================
  void cfwShowResults(CFWResults* pRes){
    fprintf(stderr, "CFWResults->cfwModel      : %d\n", pRes->cfwModel);
    fprintf(stderr, "CFWResults->cfwPosition   : %d\n", pRes->cfwPosition);
    fprintf(stderr, "CFWResults->cfwStatus     : %d\n", pRes->cfwStatus);
    fprintf(stderr, "CFWResults->cfwError      : %d\n", pRes->cfwError);
  }
  
  
  void sbig::filter_wheel_func(const Nan::FunctionCallbackInfo<v8::Value>& args){
    
    const char* usage="usage: filter_wheel(wheel_position_integer)";
    v8::Isolate* isolate = args.GetIsolate();

    if (args.Length() != 1) {

      isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, usage)));
      return;
    }

    
    sbig* obj = Nan::ObjectWrap::Unwrap<sbig>(args.This());

    v8::Local<v8::Number> pos=v8::Local<v8::Number>::Cast(args[0]);
    
    unsigned long  position= (unsigned long) pos->Value();
    unsigned short cfwModel;
    
    switch (obj->pcam->GetCameraType()){
    case 	STL_CAMERA:
      cfwModel = CFWSEL_CFWL;
      break;
      
    case	STF_CAMERA:
      cfwModel = CFWSEL_FW5_8300;
      break;
      
    default:
      cfwModel = CFWSEL_AUTO;
      break;
    };

    PAR_ERROR	 err;
    CFWResults cfwr;

    err = cfwInit(cfwModel, &cfwr);
    
    fprintf(stderr, "----------------------------------------------\n");
    fprintf(stderr, "cfwInit err: %d\n", err);

    if (err != CE_NO_ERROR){
      isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, "Error initializing filter wheel !")));
      args.GetReturnValue().Set(args.This());
      return;
    }

    cfwModel = cfwr.cfwModel;
    
    sleep(5);
    
    // cfwGoto
    //position = 3;
    err = cfwGoto(cfwModel, position, &cfwr);
    fprintf(stderr, "----------------------------------------------\n");
    fprintf(stderr, "cfwGoto requested position: %ld, err: %d\n", position, err);
    
    if (err != CE_NO_ERROR){
      isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, usage)));
      args.GetReturnValue().Set(args.This());
      return;
    }
    
    cfwShowResults(&cfwr);
    args.GetReturnValue().Set(args.This());
  }
  
  void usb_info_func(const Nan::FunctionCallbackInfo<v8::Value>& args){

    v8::Isolate* isolate = args.GetIsolate();
    
    //const char* usage="usage: usb_info( callback_function )";


    const char* usage="usage: usb_info(callback_function )";

    if (args.Length() != 1) {
      isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, usage)));
      return;
    }

    //    sbig* obj = Nan::ObjectWrap::Unwrap<sbig>(args.This());
    //v8::Local<v8::Function> ccb=v8::Local<v8::Function>::Cast(args[0]);    

    
    //    v8::Local<Nan::Callback> ccb =Nan::New<Nan::Callback>();
    v8::Local<v8::Function> ccb = v8::Local<v8::Function>::Cast(args[0]);

    //Nan::Callback ncb;
    //v8::Local<Nan::Callback> cb = Nan::New<Nan::Callback>(ncb);
    //cb->SetFunction(ccb);
    
    //send_status( ccb,"info","Initializing camera...","init");
    QueryUSBResults usb_results;
    QUERY_USB_INFO usb_i;
    
    try{

      sbig_cam* pcam=new sbig_cam(NULL);
      pcam->OpenDriver();
      
      usb_results=usb_info();
      pcam->CloseDriver();
      delete pcam;
      
      const unsigned argc = 1;

      int ncams=usb_results.camerasFound;
      v8::Handle<v8::Array> cameras = v8::Array::New(isolate, ncams);
      stringstream ss;
      for(int i=0,k=0;i<4;i++){
	usb_i=usb_results.usbInfo[i];
	
	if(usb_i.cameraFound){
	  ss.str("");
	  ss<<"DEV_USB"<<(i+1);
	  v8::Handle<v8::Object> msg = v8::Object::New(isolate);
	  msg->Set(v8::String::NewFromUtf8(isolate, "id"),v8::Number::New(isolate, i ));
	  msg->Set(v8::String::NewFromUtf8(isolate, "dev"),v8::String::NewFromUtf8(isolate, ss.str().c_str() ));
	  msg->Set(v8::String::NewFromUtf8(isolate, "name"),v8::String::NewFromUtf8(isolate, usb_i.name));
	  msg->Set(v8::String::NewFromUtf8(isolate, "serial"),v8::String::NewFromUtf8(isolate, usb_i.serialNumber));  
	  
	  cameras->Set(k, msg);
	  k++;
	}
      }
      
      
      v8::Handle<v8::Value> msgv(cameras);
      v8::Handle<v8::Value> argv[argc] = { msgv };
      
      ccb->Call(isolate->GetCurrentContext()->Global(), argc, argv );    
    }
    
    catch (qk::exception& e){
      //send_status( cb,"error",e.mess,"init");
    }
    
    args.GetReturnValue().Set(args.This());
  }

  void sbig::shutdown_func(const Nan::FunctionCallbackInfo<v8::Value>& args){
      //v8::Isolate* isolate = args.GetIsolate();
    
    sbig* obj = Nan::ObjectWrap::Unwrap<sbig>(args.This());




    //Nan::Callback* cb =new Nan::Persistent<Nan::Callback>(*obj->edata.emit); //Nan::New<>(); //v8::Local<v8::Function>::Cast(args[0]);
    
    v8::Local<v8::Function> cb = v8::Local<v8::Function>::Cast(args[0]);

    send_status_func(cb,"info","Camera driver unloading","init");
    
    try{
      obj->kill_thread();
      obj->shutdown();
      send_status_func(cb, "success","Camera driver unloaded","init");
    }
    catch (qk::exception& e){
     send_status_func(cb, "error",e.mess,"init");
    }

    //    delete cb;
    
    args.GetReturnValue().Set(args.This());
    
  }

  void sbig::shutter_func(const Nan::FunctionCallbackInfo<v8::Value>& args){
    v8::Isolate* isolate = args.GetIsolate();
    
    const char* usage="usage: initialize( device )";

    if (args.Length() != 1) {
      isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, usage)));
      return;
    }
    
    sbig* obj = Nan::ObjectWrap::Unwrap<sbig>(args.This());

    auto resolver = v8::Promise::Resolver::New(isolate);
    auto promise = resolver->GetPromise();

    obj->AW.resolver=v8::Persistent<v8::Promise::Resolver, v8::CopyablePersistentTraits<v8::Promise::Resolver>>(isolate, resolver);
    //obj->AW.handlers.push_back(v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>>(isolate, cb));

    
    v8::Local<v8::Number> usb_id=v8::Local<v8::Number>::Cast(args[0]);    

    try{
      //double uid=usb_id->Value();
      cam_command* cc=new cam_command(COM_SHUTTER);
      cc->args.push_back(usb_id->Value());
      
      std::unique_lock<std::mutex> lock(obj->m);

      obj->command_queue.push(cc);
      obj->notified = true;
      

      //cout << "DGrab async...this is " << obj << endl;
      //uv_rwlock_wrlock(&obj->AW.lock);
      //cout << "DGrab async...locked" << endl;
      //obj->AW.msgArr.push_back("HELLOOOOO from startfunc");
      //uv_rwlock_wrunlock(&obj->AW.lock);
      //cout << "DGrab async..unlocked." << endl;

      obj->cond_var.notify_one();
      


      //      obj->initialize(usb_id->Value());
      //    send_status_cb(cb,"success","Camera is ready","init");
    }
    catch (qk::exception& e){
      resolver->Reject(Nan::New("config_cam: " + e.mess).ToLocalChecked());
      //      send_status_cb(cb,"error",e.mess,"init");
    }

    args.GetReturnValue().Set(promise);

  }
  
  void sbig::initialize_func(const Nan::FunctionCallbackInfo<v8::Value>& args){

    
    v8::Isolate* isolate = args.GetIsolate();
    
    const char* usage="usage: initialize( device )";

    if (args.Length() != 1) {
      isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, usage)));
      return;
    }
    
    sbig* obj = Nan::ObjectWrap::Unwrap<sbig>(args.This());

    auto resolver = v8::Promise::Resolver::New(isolate);
    auto promise = resolver->GetPromise();

    //obj->AW.handlers.push_back(v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>>(isolate, cb));

    obj->AW.resolver=v8::Persistent<v8::Promise::Resolver, v8::CopyablePersistentTraits<v8::Promise::Resolver>>(isolate, resolver);
    
    v8::Local<v8::Number> usb_id=v8::Local<v8::Number>::Cast(args[0]);    

    try{
      //double uid=usb_id->Value();
      cam_command* cc=new cam_command(COM_INITIALIZE);
      cc->args.push_back(usb_id->Value());
      
      std::unique_lock<std::mutex> lock(obj->m);

      obj->command_queue.push(cc);
      obj->notified = true;
      

      //cout << "DGrab async...this is " << obj << endl;
      //uv_rwlock_wrlock(&obj->AW.lock);
      //cout << "DGrab async...locked" << endl;
      //obj->AW.msgArr.push_back("HELLOOOOO from startfunc");
      //uv_rwlock_wrunlock(&obj->AW.lock);
      //cout << "DGrab async..unlocked." << endl;

      obj->cond_var.notify_one();
      


      //      obj->initialize(usb_id->Value());
      //    send_status_cb(cb,"success","Camera is ready","init");
    }
    catch (qk::exception& e){
      resolver->Reject(Nan::New("initialize_func: " + e.mess).ToLocalChecked());
      //      send_status_cb(cb,"error",e.mess,"init");
    }

    args.GetReturnValue().Set(promise);
    //    args.GetReturnValue().Set(args.This());
  }

  
  // static AsyncWork* AW = new AsyncWork();

  // std::queue<int> produced_nums;
  // std::mutex m;
  // std::condition_variable cond_var;
  // bool done = false;
  // bool notified = false;
  

  // void tfunc(){
  //   std::this_thread::sleep_for(std::chrono::seconds(5));
  //   for (int i=0;i<10;i++){
  //     cout << "Hello from thread ! i= "<<i << endl;
  //     std::this_thread::sleep_for(std::chrono::seconds(1));
  //     std::unique_lock<std::mutex> lock(m);
  //     std::cout << "producing " << i << '\n';
  //     produced_nums.push(i);
  //     notified = true;


  //     cout << "Grab async..." << endl;
  //     uv_rwlock_wrlock(&AW->lock);
  //     cout << "Grab async...locked" << endl;
  //     AW->msgArr.push_back("HELLOOOOO");
  //     uv_rwlock_wrunlock(&AW->lock);
  //     cout << "Grab async..unlocked." << endl;
  //     // Wakeup the event loop to handle stored messages.
  //     // In this example, the function "invokeHandlers" will be called then.
  //     uv_async_send(&AW->async);
    
  //     cout << "Grab async...sent!" << endl;
      
      
      
  //     cond_var.notify_one();
  //   }
  //   done = true;
  //   cond_var.notify_one();
  // }

  // //  std::thread T(tfunc);

  // void cam_tfunc(sbig* o);

  // class cam_object {
  // public:
  //   cam_object(sbig* sb):sbg(sb){
  //     //      T = new std::thread(cam_tfunc, this);
  //   }
  //   ~cam_object(){}
  //   void exec(){
    
  //     std::unique_lock<std::mutex> lock(m);
  //     while (!done) {
  // 	while (!notified) {  // loop to avoid spurious wakeups
  // 	  cond_var.wait(lock);
  //           }   
  // 	while (!produced_nums.empty()) {
  // 	  std::cout << "consuming " << produced_nums.front() << '\n';
  // 	  produced_nums.pop();
  // 	}   
  // 	notified = false;
  //     }   
      
  //   }
  //   sbig* sbg;

    
  //   std::queue<int> produced_nums;
  //   std::thread* T;
  //   std::mutex m;
  //   std::condition_variable cond_var;
  //   bool done = false;
  //   bool notified = false;
    
  // };

  void cam_tfunc(sbig* o){
    o->exec();
  }

  void grab_events (uv_async_t *handle);
  
  sbig::sbig():

    
    pcam(0),
    //    expt(this),    
    T(NULL),
    infinite_loop(false),
    continue_expo(0){

    cout << "SBIG: CTOR" << endl;
    
    width=0;
    height=0;
    uv_async_init(uv_default_loop(), &this->AW.async, grab_events);    
    T = new std::thread(cam_tfunc, this);
    AW.obj_persistent=NULL;

  }
  
  sbig::~sbig(){
    try{
      cout << "SBIG DTOR: Delete " << this << endl;
      kill_thread();
      shutdown();
    } 
    catch(qk::exception& e){
      MERROR<< e.mess << endl;
    }
  }

  void sbig::kill_thread(){
    if(T==NULL)return;

    
    done=true;
    notified = true;
    MINFO << "Camera thread kill notify"<<endl;
    cond_var.notify_one();
    MINFO << "JOIN Camera thread !"<<endl;
    T->join();
    delete T;
    T=NULL;
    uv_close((uv_handle_t*)&this->AW.async, [](uv_handle_t* handle) {
        // My async callback here
        //free(handle);
      });
    
  }

  std::map<int, std::function<void(cam_command*,sbig*)>> cam_handlers;
  
  void sbig::exec(){

    cout << "SBIG Thread startup "<< this << endl;
    std::unique_lock<std::mutex> lock(m);
    //cam_event* came;
      
      while (!done) {
	while (!notified) {  // loop to avoid spurious wakeups
	  cout << "SBig: Waiting" << endl;
	  cond_var.wait(lock);
	}   
	while (!command_queue.empty()) {
	  std::cout << "SBIG Thread consuming " << command_queue.front() << '\n';
	  cam_command* com=command_queue.front();
	  command_queue.pop();
	  cam_handlers[com->command](com, this);
	  delete com;
	}   
	notified = false;
      }   
      //      cout << "SBIG Thread FINISHED! "<< this << endl;
      
  }
  

  void setup_cam_handlers(){
    cam_handlers.insert(std::make_pair(COM_INITIALIZE,[](cam_command* com,sbig* obj){
	  cam_event* came;
	  came=new cam_event();
	  came->obj=obj;
	  came->event=EVT_INIT_REPORT;
	  
	  try{
	    obj->initialize(com->args[0]);
	    came->title="success";
	    came->message="Camera is ready";
	  }
	  catch (qk::exception& e){
	    came->title="error";
	    came->message=e.mess;
	  }
	  obj->event_queue.push(came);
	  obj->AW.async.data = obj;
	  uv_async_send(&obj->AW.async);
	}
	));

    cam_handlers.insert(std::make_pair(COM_EXPO,[](cam_command* com,sbig* obj){
	  cam_event* came;
	  came=new cam_event();
	  came->obj=obj;
	  came->event=EVT_EXPO_COMPLETE;

	  try{

	    if(!obj->pcam) throw qk::exception("No PCAM !");
	    
	    SBIG_DARK_FRAME sbdf= obj->light_frame ? SBDF_LIGHT_ONLY : SBDF_DARK_ONLY;
	    
	    obj->pcam->EstablishLink();
	    obj->check_error();
	    
	    CSBIGImg *pImg= 0;    
	    pImg = new CSBIGImg;
	    pImg->AllocateImageBuffer(obj->height, obj->width);
	    
	    
	    
	    int expo=0;
	    
	    //    continue_expo_mut.lock();
	    int continue_expo=1;
	    
	    obj->check_error();
	    
	    while(continue_expo){
	      
	      MINFO << "Accumulating photons. Exptime="<<obj->exptime << " Expo  "<< (expo+1) << "/" << obj->nexpo<<endl;
	      
	      //void CSBIGCam::GetGrabState(GRAB_STATE &grabState, double &percentComplete)
	      
	      obj->pcam->GrabImage(pImg,sbdf);
	      obj->check_error();
	      
	      
	      //new_event.lock();
	      
	      obj->last_image.redim(pImg->GetWidth(), pImg->GetHeight());    
	      obj->last_image.rawcopy(pImg->GetImagePointer(),obj->last_image.dim);
	      
	      expo++;

	      cam_event* came2=new cam_event();
	      came2->title="new_image";
	      came2->message="New image received";
	      came2->obj=obj;
	      came2->event=EVT_NEW_IMAGE;
	      obj->event_queue.push(came2);
	      obj->AW.async.data = obj;
	      uv_async_send(&obj->AW.async);
	      
	      if(expo>=obj->nexpo) continue_expo=0;
	      
	      //system("ds9 grab.fits -frame refresh");
	      
	    }
	    
	    continue_expo=0;
	    //    continue_expo_mut.unlock();
	    // MINFO << " Done exposures. Forcing closing of shutter." << endl;
	    
	    delete pImg;
	    obj->close_shutter();
	    
	    
	    
	    
	    //obj->really_take_exposure();
	    came->title="success";
	    came->message="All Exposures terminated";
	    
	    
	  }
	  catch(qk::exception& e){
	    came->title="error";
	    came->message=e.mess;
	    MERROR << "Error in exposure thread :" << e.mess << endl;
	    }
	  obj->event_queue.push(came);
	  obj->AW.async.data = obj;
	  uv_async_send(&obj->AW.async);
	}));

    cam_handlers.insert(std::make_pair(COM_MONITOR,[](cam_command* com,sbig* obj){

	  cam_event* came;
	  came=new cam_event();
	  came->obj=obj;
	  came->event=EVT_EXPO_COMPLETE;
	  int expo=0;
	  
	  try{
	    //obj->monitor();
	    came->title="success";
	    came->message="Monitor terminated";
	    qk::mat<unsigned short> data;

	    obj->continue_expo=1;
	    
	    obj->check_error();
	    obj->pcam->GrabSetup();
	    while(obj->continue_expo){
	      
	      //MINFO << "Accumulating photons. Exptime="<<exptime << " Expo  "<< (expo+1) << "/" << nexpo<<endl;
	      
	      //void CSBIGCam::GetGrabState(GRAB_STATE &grabState, double &percentComplete)
	      
	      obj->pcam->GrabMainFast(obj->last_image);
	      obj->check_error();
	      
	      
	      //new_event.lock();
	      //obj->last_image=data;
	      //	      last_image.redim(pImg->GetWidth(), pImg->GetHeight());    
	      //last_image.rawcopy(pImg->GetImagePointer(),last_image.dim);
	      cam_event* came2=new cam_event();
	      came2->obj=obj;
	      came2->event=EVT_NEW_IMAGE;
	      came2->title="new_image_monitor";
	      came2->message="New image received from monitor";
	      obj->event_queue.push(came2);
	      obj->AW.async.data = obj;
	      uv_async_send(&obj->AW.async);
	      
	      expo++;
	      
	    }
	    
	    obj->continue_expo=0;
	    obj->close_shutter();
	  }
	  catch(qk::exception& e){
	    came->title="error";
	    came->message=e.mess;
	    MERROR << "Error in exposure thread :" << e.mess << endl;
	  }
	  obj->event_queue.push(came);
	  obj->AW.async.data = obj;
	  uv_async_send(&obj->AW.async);
	}));
    
    cam_handlers.insert(std::make_pair(COM_SHUTTER,[](cam_command* com,sbig* obj){
	  cam_event* came;
	  came=new cam_event();
	  came->obj=obj;
	  came->event=EVT_SHUTTER;
	  
	  try{
	    MINFO << "Changing shutter to position " << com->args[0] << endl;
	    //obj->initialize(com->args[0]);
	    
	    came->title="success";
	    came->message="Shutter moved";
	  }
	  catch (qk::exception& e){
	    came->title="error";
	    came->message=e.mess;
	  }
	  obj->event_queue.push(came);
	  obj->AW.async.data = obj;
	  uv_async_send(&obj->AW.async);
	}
	));

  }

  
  
  std::map<int, std::function<void(cam_event*,sbig*,v8::Isolate*)>> event_handlers;

  void setup_events(){

    event_handlers.insert(std::make_pair(EVT_INIT_REPORT,[](cam_event* cevent, sbig* obj, v8::Isolate* isolate){
	if(!obj->AW.resolver.IsEmpty()){
	  auto resolver=v8::Local<v8::Promise::Resolver>::New(isolate, obj->AW.resolver);
	  v8::Handle<v8::Object> msg = Nan::New<v8::Object>();//.ToLocalChecked(); 
	  msg->Set(Nan::New<v8::String>("type").ToLocalChecked(),Nan::New<v8::String>(cevent->title).ToLocalChecked());
	  msg->Set(Nan::New<v8::String>("content").ToLocalChecked(),Nan::New<v8::String>(cevent->message).ToLocalChecked());  
	    
	  if(cevent->title=="error"){
	      
	    resolver->Reject(msg);
	  }
	  else{
	    MINFO << "Resolving INIT promise " << endl;
	    resolver->Resolve(msg);      
	  }
	    
	  obj->AW.resolver.Reset();
	}
	  
	}));
      
    event_handlers.insert(std::make_pair(EVT_SHUTTER,[](cam_event* cevent, sbig* obj, v8::Isolate* isolate){
	if(!obj->AW.resolver.IsEmpty()){
	  auto resolver=v8::Local<v8::Promise::Resolver>::New(isolate, obj->AW.resolver);
	  v8::Handle<v8::Object> msg = Nan::New<v8::Object>();//.ToLocalChecked(); 
	  msg->Set(Nan::New<v8::String>("type").ToLocalChecked(),Nan::New<v8::String>(cevent->title).ToLocalChecked());
	  msg->Set(Nan::New<v8::String>("content").ToLocalChecked(),Nan::New<v8::String>(cevent->message).ToLocalChecked());  
	    
	  if(cevent->title=="error"){
	      
	    resolver->Reject(msg);
	  }
	  else{
	    resolver->Resolve(msg);      
	  }
	    
	  obj->AW.resolver.Reset();
	}
	
	}));
    
    
    
    event_handlers.insert(std::make_pair(EVT_EXPO_COMPLETE,[](cam_event* cevent, sbig* obj, v8::Isolate* isolate){
	  if(!obj->AW.resolver.IsEmpty()){
	    auto resolver=v8::Local<v8::Promise::Resolver>::New(isolate, obj->AW.resolver);
	    v8::Handle<v8::Object> msg = Nan::New<v8::Object>();//.ToLocalChecked(); 
	    msg->Set(Nan::New<v8::String>("type").ToLocalChecked(),Nan::New<v8::String>(cevent->title).ToLocalChecked());
	    msg->Set(Nan::New<v8::String>("content").ToLocalChecked(),Nan::New<v8::String>(cevent->message).ToLocalChecked());  
	    
	    if(cevent->title=="error"){
	      
	      resolver->Reject(msg);
	    }
	    else{
	      resolver->Resolve(msg);      
	    }
	    
	    obj->AW.resolver.Reset();
	    obj->AW.event_callback.Reset();
	    obj->AW.obj_persistent->Reset();

	    MINFO << "Sent EXPO_COMPLETE " << cevent->message << endl;
	    //obj->kill_thread();
	    
      
	  }
	  
	}));
    event_handlers.insert(std::make_pair(EVT_NEW_IMAGE,[](cam_event* cevent, sbig* obj, v8::Isolate* isolate){
	if(!obj->AW.event_callback.IsEmpty()){

	  auto cb=v8::Local<v8::Function>::New(isolate, obj->AW.event_callback);

	  v8::Local<v8::Object> obj_pers;
	  if(obj->AW.obj_persistent!=NULL)
	    obj_pers=Nan::New(*obj->AW.obj_persistent);
	  
	  v8::Local<v8::Function> jsu_cons = Nan::New<v8::Function>(jsmat<unsigned short>::constructor());
	  v8::Local<v8::Object> jsm = jsu_cons->NewInstance(Nan::GetCurrentContext()).ToLocalChecked();
	  v8::Handle<v8::Object> jsmo = v8::Handle<v8::Object>::Cast(jsm);

	  if(!jsmo.IsEmpty()){
	    
	    //  cout << "Hello" << endl;
	    v8::Handle<v8::Value> fff=obj_pers->Get(Nan::New<v8::String>("last_image").ToLocalChecked());
	    jsmat<unsigned short>* last_i = Nan::ObjectWrap::Unwrap<jsmat<unsigned short> >(v8::Handle<v8::Object>::Cast(fff));
	    jsmat<unsigned short>* jsmv_unw = Nan::ObjectWrap::Unwrap<jsmat<unsigned short> >(v8::Handle<v8::Object>::Cast(jsmo));
	    
	    (*jsmv_unw)=obj->last_image;
	    //	    cout << "Hello COPY 2" << obj->last_image.dims[0] << ", "<< obj->last_image.dims[1] << "  " << endl;
	    (*last_i)=obj->last_image;
	    //cout << "COPY OK w="<< jsmv_unw->dims[0] << endl;
	    
	    // v8::Handle<v8::Value> h_fimage=obj_pers->Get(Nan::New<v8::String>("last_image_float").ToLocalChecked());
      	    // jsmat<float>* fimage = Nan::ObjectWrap::Unwrap<jsmat<float> >(v8::Handle<v8::Object>::Cast(h_fimage)); //new jsmat<unsigned short>();
      	    // fimage->redim(last_i->dims[0],last_i->dims[1]);
      	    // //MINFO << "Copy float image DIMS " << last_i->dims[0] << ", " << last_i->dims[1] << endl;
      	    // for(int p=0;p<fimage->dim;p++)fimage->c[p]=(float)last_i->c[p];

	    
      	    const unsigned argc = 1;
	    
      	    v8::Handle<v8::Object> msg = Nan::New<v8::Object>();
      	    msg->Set(Nan::New<v8::String>("type").ToLocalChecked(),Nan::New<v8::String>("new_image").ToLocalChecked());
      	    //msg->Set(Nan::New<v8::String>("content").ToLocalChecked(),h_fimage);
	    msg->Set(Nan::New<v8::String>("content").ToLocalChecked(),jsmo);  
      	    //if(id!="")
      	    msg->Set(Nan::New<v8::String>( "id").ToLocalChecked(),Nan::New<v8::String>( "expo_proc").ToLocalChecked());  
      	    v8::Handle<v8::Value> msgv(msg);
      	    v8::Handle<v8::Value> argv[argc] = { msgv };

	    MINFO << "NEW_IMAGE "  << endl;


      	    cb->Call(isolate->GetCurrentContext()->Global(), argc, argv );    


      	    /*
	      v8::Handle<v8::Object> msg = v8::Object::New();
	      msg->Set(v8::String::NewFromUtf8(isolate, "new_image"),h_fimage);  
	      //v8::Handle<v8::Value> msgv(msg);
	    
	      Handle<v8::Value> argv[1] = { msg };
	      obj->cb->Call(v8::Context::GetCurrent()->Global(), 1, argv );    
      	    */

      	  }else{
      	    cout << "BUG ! empty handle !"<<endl;
      	  }
	}
	  
	  
	}));
    event_handlers.insert(std::make_pair(EVT_EXPO_PROGRESS,[](cam_event* cevent, sbig* obj, v8::Isolate* isolate){
	if(!obj->AW.event_callback.IsEmpty()){

	  auto cb=v8::Local<v8::Function>::New(isolate, obj->AW.event_callback);
	  
	  std::map<int,std::string>::iterator it=cam_events.find(cevent->event);
	  std::string event_name;
	  if(it != cam_events.end()){
	    event_name = it->second;
	  }

	  const unsigned argc = 1;
	
	  v8::Handle<v8::Object> msg = Nan::New<v8::Object>();
	  msg->Set(Nan::New<v8::String>("type").ToLocalChecked(),Nan::New<v8::String>(event_name).ToLocalChecked());
	  msg->Set(Nan::New<v8::String>("content").ToLocalChecked(),Nan::New<v8::Number>(cevent->complete));  
	  msg->Set(Nan::New<v8::String>( "id").ToLocalChecked(),Nan::New<v8::String>( "expo_proc").ToLocalChecked());  
	  v8::Handle<v8::Value> msgv(msg);
	  v8::Handle<v8::Value> argv[argc] = { msgv };
	
	  MINFO << "EXPO_PROGRESS " << cevent->complete << endl;
	  cb->Call(isolate->GetCurrentContext()->Global(), argc, argv );    
	}	  
	}));
    event_handlers.insert(std::make_pair(EVT_GRAB_PROGRESS,[](cam_event* cevent, sbig* obj, v8::Isolate* isolate){
	if(!obj->AW.event_callback.IsEmpty()){

	  auto cb=v8::Local<v8::Function>::New(isolate, obj->AW.event_callback);

	  std::map<int,std::string>::iterator it=cam_events.find(cevent->event);
	  std::string event_name;
	  if(it != cam_events.end()){
	    event_name = it->second;
	  }
	  
	  const unsigned argc = 1;
	
	  v8::Handle<v8::Object> msg = Nan::New<v8::Object>();
	  msg->Set(Nan::New<v8::String>("type").ToLocalChecked(),Nan::New<v8::String>(event_name).ToLocalChecked());
	  msg->Set(Nan::New<v8::String>("content").ToLocalChecked(),Nan::New<v8::Number>(cevent->complete));  
	  msg->Set(Nan::New<v8::String>( "id").ToLocalChecked(),Nan::New<v8::String>( "expo_proc").ToLocalChecked());  
	  v8::Handle<v8::Value> msgv(msg);
	  v8::Handle<v8::Value> argv[argc] = { msgv };

	  MINFO << "GRAB_PROGRESS " << cevent->complete << endl;
	  //cout << "Emit Image event " << endl;
	  cb->Call(isolate->GetCurrentContext()->Global(), argc, argv );    
	}	  
	}));

  }
  
  void grab_events (uv_async_t *handle) {

    //cam_event* cevent=static_cast<cam_event*>(handle->data);
    v8::Isolate * isolate = v8::Isolate::GetCurrent();

    Nan::HandleScope scope;
    sbig* obj=static_cast<sbig*>(handle->data);
    //    sbig* obj=(sbig*) cevent->obj;


    v8::HandleScope handleScope(isolate); 
    
    while(!obj->event_queue.empty()){
      
      cam_event* cevent=obj->event_queue.front();
      obj->event_queue.pop();
      
      std::map<int,std::string>::iterator it=cam_events.find(cevent->event);
      std::string event_name;
      if(it != cam_events.end()){
	event_name = it->second;
      }

      //      MINFO << "Calling handlers for " << event_name << endl;
      event_handlers[cevent->event](cevent, obj, isolate);
      //MINFO << "Calling handlers for " << event_name << "done" << endl;
      delete cevent;
    }
      
      
  }



  void sbig::config_cam(v8::Local<v8::Object>& options){
    v8::Isolate *isolate = v8::Isolate::GetCurrent();
    cout << "Config cam..." << endl;
    
    pcam->SetActiveCCD(CCD_IMAGING);

    check_error();
    
    v8::Local<v8::Value> fff=options->Get(v8::String::NewFromUtf8(isolate, "exptime"));
    
    if(!fff->IsUndefined()){
      cout << "Config cam set exptime" << endl;
      pcam->SetExposureTime(fff->NumberValue());
      check_error();
      exptime = fff->NumberValue();

    }
    


    
    fff=options->Get(v8::String::NewFromUtf8(isolate, "nexpo"));
    
    if(!fff->IsUndefined()){
      nexpo = fff->NumberValue();
    }
    cout << "Config cam set readout" << endl;
    
    fff=options->Get(v8::String::NewFromUtf8(isolate, "fast_readout"));
    if(!fff->IsUndefined()){
      
      pcam->SetFastReadout(fff->NumberValue());
      check_error();
    }
    
    fff=options->Get(v8::String::NewFromUtf8(isolate, "dual_channel_mode"));
    if(!fff->IsUndefined()){
      pcam->SetDualChannelMode(fff->NumberValue());
      check_error();
    }


    fff=options->Get(v8::String::NewFromUtf8(isolate, "light_frame"));
    if(!fff->IsUndefined()){
      light_frame = fff->BooleanValue();
      check_error();
    }

    cout << "Config cam set readout" << endl;

    int rm=0;
    int top=0, left=0, fullWidth, fullHeight;
    
    fff=options->Get(v8::String::NewFromUtf8(isolate, "readout_mode"));
    
    if(!fff->IsUndefined()){
      v8::String::Utf8Value param1(fff->ToString());
      
      // convert it to string
      std::string foo = std::string(*param1);  
      //readout mode
      rm = 0; //suppose 1x1
      if (strcmp(foo.c_str(), "2x2") == 0){
	rm = 1;
      }
      else if (strcmp(foo.c_str(), "3x3") == 0){
	rm = 2;
      }
      
      pcam->SetReadoutMode(rm);
      check_error();
    }

    pcam->EstablishLink();
    pcam->GetFullFrame(fullWidth, fullHeight);
    check_error();
    MINFO << "FullFrame : " << fullWidth << ", " << fullHeight << endl;

    v8::Local<v8::Array> subframe_array=v8::Local<v8::Array>::Cast(options->Get(v8::String::NewFromUtf8(isolate, "subframe")));
    if(!subframe_array->IsUndefined()){
      
      v8::Local<v8::Number> n;
      n= v8::Local<v8::Number>::Cast(subframe_array->Get(0));left=n->Value();
      n= v8::Local<v8::Number>::Cast(subframe_array->Get(1));top=n->Value();
      n= v8::Local<v8::Number>::Cast(subframe_array->Get(2));width=n->Value();
      n= v8::Local<v8::Number>::Cast(subframe_array->Get(3));height=n->Value();

      MINFO << "subframe Left=" << left << ", Top="<< top<< ", Width="<< width<< ", Height="<< height <<endl;
    }else{
      width = fullWidth;
      height = fullHeight;
      left=0;
      top=0;
    }
    //cout << "Config cam get full frame..." << endl;

    
    if (width == 0)width = fullWidth;    
    if (height == 0)height = fullHeight;
    pcam->SetSubFrame(left, top, width, height);
    check_error();
  }
  
  void sbig::start_exposure_func(const Nan::FunctionCallbackInfo<v8::Value>& args){
    

      
      //    MINFO << "ArgsLength " << args.Length() << endl;
    const char* usage="usage: start_exposure( {options},  callback_function )";
    v8::Isolate *isolate = v8::Isolate::GetCurrent();
    if (args.Length() != 2) {
      isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, usage)));
      return;
      
    }

    sbig* obj = Nan::ObjectWrap::Unwrap<sbig>(args.This());
    auto resolver = v8::Promise::Resolver::New(isolate);
    auto promise = resolver->GetPromise();

    //    v8::Local<v8::Object> thisobj=v8::Local<v8::Object>::Cast(args.This());
    v8::Local<v8::Object> options=v8::Local<v8::Function>::Cast(args[0]);
    v8::Local<v8::Function> cb=v8::Local<v8::Function>::Cast(args[1]);
    //v8::Local<v8::Function> cb=To<v8::Function>(args[1]).ToLocalChecked(); 
      
    //v8::Local<v8::Function> cb_emit=v8::Local<v8::Function>::Cast(args[2]);

    args.GetReturnValue().Set(promise);

    cout << "set promise done..." << endl;
     
     cout << "queue work..." << endl;

     try{
       obj->config_cam(options);
     }
     catch(qk::exception& e){
       resolver->Reject(Nan::New("config_cam: " + e.mess).ToLocalChecked());
       return;
     }
     v8::Local<v8::Object> thisobj=args.This();
      
    obj->AW.event_callback=v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>>(isolate, cb);
    obj->AW.resolver=v8::Persistent<v8::Promise::Resolver, v8::CopyablePersistentTraits<v8::Promise::Resolver>>(isolate, resolver);
    
    obj->AW.obj_persistent=new Nan::Persistent<v8::Object>(thisobj);
    
    MINFO << "Send status....OK" << endl;

    //double uid=usb_id->Value();
    cam_command* cc=new cam_command(COM_EXPO);
    //    cc->args.push_back(usb_id->Value());
    
    std::unique_lock<std::mutex> lock(obj->m);
    
    obj->command_queue.push(cc);
    obj->notified = true;
    obj->cond_var.notify_one();
    
    cout << "expo func done " << endl;
    

  }

  void sbig::monitor_func(const Nan::FunctionCallbackInfo<v8::Value>& args){
    const char* usage="usage: start_exposure( {options},  callback_function )";
    v8::Isolate *isolate = v8::Isolate::GetCurrent();
    if (args.Length() != 2) {
      isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, usage)));
      return;
      
    }

    sbig* obj = Nan::ObjectWrap::Unwrap<sbig>(args.This());
    auto resolver = v8::Promise::Resolver::New(isolate);
    auto promise = resolver->GetPromise();

    //    v8::Local<v8::Object> thisobj=v8::Local<v8::Object>::Cast(args.This());
    v8::Local<v8::Object> options=v8::Local<v8::Function>::Cast(args[0]);
    v8::Local<v8::Function> cb=v8::Local<v8::Function>::Cast(args[1]);
    //v8::Local<v8::Function> cb=To<v8::Function>(args[1]).ToLocalChecked(); 
      
    //v8::Local<v8::Function> cb_emit=v8::Local<v8::Function>::Cast(args[2]);
    
    args.GetReturnValue().Set(promise);
    obj->config_cam(options);
    v8::Local<v8::Object> thisobj=args.This();
    
    obj->AW.event_callback=v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>>(isolate, cb);
    obj->AW.resolver=v8::Persistent<v8::Promise::Resolver, v8::CopyablePersistentTraits<v8::Promise::Resolver>>(isolate, resolver);
    
    obj->AW.obj_persistent=new Nan::Persistent<v8::Object>(thisobj);
    
    MINFO << "Monitor: sending command" << endl;

    //double uid=usb_id->Value();
    cam_command* cc=new cam_command(COM_MONITOR);
    //    cc->args.push_back(usb_id->Value());
    
    std::unique_lock<std::mutex> lock(obj->m);
    
    obj->command_queue.push(cc);
    obj->notified = true;
    obj->cond_var.notify_one();
    
    cout << "Monitor func done " << endl;
    
    return;
    
  }

  
  void sbig::stop_exposure_func(const Nan::FunctionCallbackInfo<v8::Value>& args){

      //v8::Isolate* isolate = args.GetIsolate();

    sbig* obj = Nan::ObjectWrap::Unwrap<sbig>(args.This());


    //    v8::Local<v8::Function> cb = v8::Local<v8::Function>::Cast(args[0]);
    // send_status_func(cb, "info","stop exposure");
    obj->stop_exposure();
    
    args.GetReturnValue().Set(args.This());
  }

  void sbig::shutdown(){
    if(!pcam) return;

    stop_exposure();

    //void *x=0;
    // MINFO << "Waiting for end of operations.." << endl;

    // new_event.lock();
    // while(expt.running){
    //   new_event.wait();

    // }
    // new_event.unlock();
    
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
  
  QueryUSBResults usb_info(){
    QueryUSBResults usb_results;
    
    SBIGUnivDrvCommand(CC_QUERY_USB, NULL, &usb_results);
    return usb_results;
  }

  // QueryUSBResults sbig::usb_info(){
  //   shutdown();

  //   pcam = new sbig_cam(this);

  //   pcam->OpenDriver();
  //   check_error();
    
  //   QueryUSBResults usb_results = pcam->usb_info();

  //   check_error();
    
  //   delete pcam;
  //   pcam=NULL;
  //   return usb_results;
  // }
  
  void sbig::initialize(int usb_id){

    MINFO << "Shutting dowm camera ... ID " << usb_id << endl;
    
    shutdown();

    MINFO << "Opening link to camera with USB ID = " << usb_id << endl;

    SBIG_DEVICE_TYPE dev= (SBIG_DEVICE_TYPE) (DEV_USB+2+usb_id);
    //pcam = new sbig_cam(this, DEV_USB1);
  
    string caminfo="";
    //double ccd_temp;
    //unsigned short regulation_enabled;
    //double setpoint_temp;
    //double percent_power;

    

    

    try{
      pcam = new sbig_cam(this, dev);
      MINFO << "Connected to camera : [" << pcam->GetCameraTypeString() << "] cam info = ["<< caminfo<<"]"<<endl;
      

      
      // pcam->QueryTemperatureStatus(regulation_enabled, ccd_temp,setpoint_temp, percent_power);
      // MINFO << "CCD Temperature regulation : " << (regulation_enabled? "ON" : "OFF")
      // 	    << ", setpoint = " << setpoint_temp <<  " °C. Current CCD temperature =  " << ccd_temp << " °C."
      // 	    << " Cooling power : "<< percent_power << "%."<<endl;

      check_error();
    }
    catch(qk::exception& e){
      MERROR << "SBIG: initialize error : "<< e.mess << endl;
    }
    
    //MINFO << "Camera init done"<<endl;

    // pcam->GetFormattedCameraInfo(caminfo, 0);      
      
    // try{
    //   check_error();
    // }
    // catch(qk::exception& e){
    //   cerr << e.mess << endl;
    // }
    
    //    pcam->GetFullFrame(ccd_width, ccd_height);

    
    
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

    //    cout << "Link Established to Camera Type: " << pcam->GetCameraTypev8::String() << endl;

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

    // continue_expo_mut.lock();
    // bool already_exposing = continue_expo;
    // continue_expo_mut.unlock();
    
    // if(already_exposing) throw qk::exception("An exposure is already taking place !, Stop it first.");

    // new_event.lock();

    // expt.start();
    // expt.running=1;

    // new_event.unlock();

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
    pImg->AllocateImageBuffer(height, width);
    

    
    int expo=0;

    //    continue_expo_mut.lock();
    continue_expo=1;

    check_error();
    
    while(continue_expo){

      MINFO << "Accumulating photons. Exptime="<<exptime << " Expo  "<< (expo+1) << "/" << nexpo<<endl;

      //void CSBIGCam::GetGrabState(GRAB_STATE &grabState, double &percentComplete)
      
      pcam->GrabImage(pImg,sbdf);
      check_error();
      

      //new_event.lock();

      last_image.redim(pImg->GetWidth(), pImg->GetHeight());    
      last_image.rawcopy(pImg->GetImagePointer(),last_image.dim);
      
      expo++;
      
      if(!infinite_loop && expo>=nexpo) continue_expo=0;

      //system("ds9 grab.fits -frame refresh");
      
    }
    
    continue_expo=0;
    //    continue_expo_mut.unlock();
    // MINFO << " Done exposures. Forcing closing of shutter." << endl;

    delete pImg;
    close_shutter();



    
  }
  

  void sbig::close_shutter(){

    MiscellaneousControlParams mcp;

    mcp.fanEnable=0;
    mcp.ledState=LED_BLINK_HIGH;
    mcp.shutterCommand=SC_CLOSE_SHUTTER;

    //    short int err;
    //err=
    pcam->SBIGUnivDrvCommand(CC_MISCELLANEOUS_CONTROL, &mcp, NULL);    
    check_error();
    
    mcp.fanEnable=0;
    mcp.ledState=LED_ON;
    mcp.shutterCommand=SC_CLOSE_SHUTTER;
    //err=
    pcam->SBIGUnivDrvCommand(CC_MISCELLANEOUS_CONTROL, &mcp, NULL);    
    check_error();

    //    MINFO  << " Shutter closed !" << endl;
  }

  /*
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
  */
  
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

      MINFO << "Connected to camera : " << pcam->GetCameraTypev8::String() << endl;

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
      MERROR << "Camera Error: " << pcam->GetErrorv8::String(err) << endl;
      if((err = pcam->CloseDevice()) != CE_NO_ERROR){       MERROR << "Camera Error: " << pcam->GetErrorv8::String(err) << endl;}
      if((err = pcam->CloseDriver()) != CE_NO_ERROR){       MERROR << "Camera Error: " << pcam->GetErrorv8::String(err) << endl;}		
      delete pcam;
      throw 2;
    }

    if((err = pcam->CloseDevice()) != CE_NO_ERROR){       MERROR << "Camera Error: " << pcam->GetErrorv8::String(err) << endl;}
    if((err = pcam->CloseDriver()) != CE_NO_ERROR){       MERROR << "Camera Error: " << pcam->GetErrorv8::String(err) << endl;}		
    
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
      MINFO << "Connected to camera : " << pcam->GetCameraTypev8::String() << endl;

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

  


  //  template <class T> Nan::Persistent<FunctionTemplate>  jsmat<T>::s_ctm;
  // template <class T> Nan::Persistent<Function> jsmat<T>::constructor;

  template class jsmat<unsigned short> ;
  template class jsmat<double> ;
  template class jsmat<float> ;
  //Nan::Persistent<FunctionTemplate> jsmat<unsigned short>::s_ctm;

  NAN_MODULE_INIT(init_node_module){
    //  void init_node_module(v8::Local<v8::Object> exports) {
    
    //colormap_interface::init(exports);
    //    cout << "Init sbig c++ plugin..." << endl;
    setup_cam_handlers();
    setup_cam_events();
    setup_events();
    
    sbig::init(target);
    sbig_driver::init(target);
    jsmat<unsigned short>::init(target,"mat_ushort");
    jsmat<float>::init(target,"mat_float");
    
    Nan::Set(target,Nan::New<v8::String>("usb_info").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(usb_info_func)).ToLocalChecked());
    //cout << "Init sbig c++ plugin done "<<endl;
  }
  
  NODE_MODULE(sbig, init_node_module)
}



