/*
  The Quarklib project.
  Written by Pierre Sprimont, <sprimont@iasfbo.inaf.it>, INAF/IASF-Bologna, 2014-2016.
  This source-code is not free, but do what you want with it anyway.
*/


#ifndef __SBIG_HH__
#define __SBIG_HH__

#include <node.h>
#include <node_object_wrap.h>
#include <node_buffer.h>

#include <string>
#include <fitsio.h>

//#include "colormap.hh"
#include "csbigcam/csbigcam.h"

#include "../node-fits/qk/mat.hh"
#include "../node-fits/qk/threads.hh"
//#include "../node-fits/colormap/colormap_interface.hh"

namespace sadira{
  
  using namespace std;
  using namespace qk;
  using namespace v8;
  using namespace node;

  class sbig;

  class sbig_cam : public CSBIGCam{

  public:

    sbig_cam(sbig* _sbig, SBIG_DEVICE_TYPE dev);
    sbig_cam(sbig* _sbig);
    
    virtual ~sbig_cam(){}

    QueryUSBResults usb_info();
    
    virtual void grab_complete(double pc);
    virtual void expo_complete(double pc);

    void check_error();
    
    sbig* sb;
  };

  QueryUSBResults usb_info(); 
  
  class sbig_driver: public node::ObjectWrap {
  public:
    static void init(Local<Object> exports);
    
  private:

    explicit sbig_driver();
    ~sbig_driver();
    
    static void New(const FunctionCallbackInfo<Value>& args);
    //static void Destructor(napi_env env, void* nativeObject, void*);

  private:
    static void initialize_camera_func(const FunctionCallbackInfo<Value>& args);
    v8::Local<v8::Function> cb;

    static Persistent<Function> constructor;
    QueryUSBResults usb_info();
  };
  
  class sbig : public node::ObjectWrap {
  public:
    static void init(Local<Object> exports);
    
    //static Persistent<FunctionTemplate> s_cts;
    
  private:

    explicit sbig();
    ~sbig();
    
    void send_status_message(Isolate* isolate, const string& type, const string& message);
    //v8::Handle<node::Buffer> gen_pngtile(v8::Handle<v8::Array>& parameters);
    
    static void New(const FunctionCallbackInfo<Value>& args);
    static void shutdown_func(const FunctionCallbackInfo<Value>& args);
    static void initialize_func(const FunctionCallbackInfo<Value>& args);
    static void start_exposure_func(const FunctionCallbackInfo<Value>& args);
    static void stop_exposure_func(const FunctionCallbackInfo<Value>& args);
    static void get_temp_func(const FunctionCallbackInfo<Value>& args);
    static void set_temp_func(const FunctionCallbackInfo<Value>& args);
    static void filter_wheel_func(const FunctionCallbackInfo<Value>& args);
    ///static void usb_info_func(const FunctionCallbackInfo<Value>& args);

    
    
    class expo_thread : public thread{
      
    public:
      expo_thread(sbig* _sbc):sbc(_sbc),running(0){}
      virtual ~expo_thread(){}
      virtual bool exec();
      sbig* sbc;
      int running;
    };

    void close_shutter();

    sbig_cam *pcam;
    string error_message;
    expo_thread expt;
    void* event;
  public:
    cond new_event;
    int event_id;
    double complete;
    static Persistent<Function> constructor;
  private:
    v8::Local<v8::Function> cb;

    
    
    /*
    REG_BASE_OBJECT sbig();
    virtual ~sbig();
    virtual void get_messages(message_info_list& minfo);
    virtual void process_message(message& incoming_message);
    */

    void check_error();
    void initialize(int usb_id);
    void start_exposure();
    void stop_exposure();
    void really_take_exposure();
    void shutdown();

    
    mat<unsigned short> last_image;
    
    bool infinite_loop;

    float exptime;
    int nexpo;
    

    int ccd_width, ccd_height, width, height;
    string camera_type_string;
    double ambient_temperature;
    double ccd_temperature;
    vec<double> ccd_pixel_size;
    double ccd_gain;
    double ccd_temperature_setpoint;
    double ccd_tempreg_power;
    int continue_expo;
    mutex continue_expo_mut;
    cond last_image_ready_cond;
    int last_image_ready;
  };

  /*  
  class sbig_ccd_temperature_control:  public command{
  public:
    istr action;
    num<double> temp;
    REG sbig_ccd_temperature_control():
      action("action","status"),
      temp("temp",-1){
      create_child(action,"",object::Static,EndProp);
      create_child(temp,"",object::Static,EndProp);
    }
    virtual ~sbig_ccd_temperature_control(){}
    virtual void command_func();
  };
  

  class sbig_ccd_exposure : public command{
  public:
    istr action,mode,file_name;
    num<double> exptime;
    num<unsigned int> readout_mode;
    REG sbig_ccd_exposure():
      action("action","grab"),
      mode("mode","light"),
      file_name("file_name","noname.fits"),
      exptime("exptime",1.0),
      readout_mode("readout_mode",1){
      
      create_child(action,"",object::Static,EndProp);
      create_child(mode,"",object::Static,EndProp);
      create_child(file_name,"",object::Static,EndProp);
      create_child(exptime,"",object::Static,EndProp);
      create_child(readout_mode,"",object::Static,EndProp);
    }
    virtual ~sbig_ccd_exposure(){}
    virtual void command_func();
    
  };
  */
  
}

#endif
