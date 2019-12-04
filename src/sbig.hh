/*
  The Quarklib project.
  Written by Pierre Sprimont, <sprimont@iasfbo.inaf.it>, INAF/IASF-Bologna, 2014-2016.
  This source-code is not free, but do what you want with it anyway.
*/


#ifndef __SBIG_HH__
#define __SBIG_HH__

//#include <node.h>
//#include <node_object_wrap.h>
//#include <node_buffer.h>

#include <string>
#include <mutex>

#include <fitsio.h>

#include <nan.h>

//#include "colormap.hh"
#include "csbigcam/csbigcam.h"

#include "../node-fits/qk/mat.hh"

#include "../node-fits/qk/threads.hh"
//#include "../node-fits/colormap/colormap_interface.hh"

namespace sadira{
  
  //using namespace std;
  using namespace qk;
  //  using namespace v8;
  //using namespace Nan;

  class sbig;

  class sbig_cam : public CSBIGCam{

  public:

    sbig_cam(sbig* _sbig, SBIG_DEVICE_TYPE dev);
    sbig_cam(sbig* _sbig);
    
    virtual ~sbig_cam(){}

    QueryUSBResults usb_info();
    
    virtual void grab_complete(double pc);
    virtual void expo_complete(double pc);

    PAR_ERROR GrabMainFast(qk::mat<unsigned short>& data);
    PAR_ERROR GrabSetupFast();
    void check_error();
    sbig* sb;

  };

  QueryUSBResults usb_info(); 
  
  class sbig_driver: public Nan::ObjectWrap {
  public:
    static void init(v8::Local<v8::Object> exports);

    static inline Nan::Persistent<v8::Function> & constructor() {
      static Nan::Persistent<v8::Function> my_constructor;
      return my_constructor;
    }


  private:

    explicit sbig_driver();
    ~sbig_driver();
    
    static void New(const Nan::FunctionCallbackInfo<v8::Value>& args);
    //static void Destructor(napi_env env, void* nativev8::Object, void*);

  private:
    static void initialize_camera_func(const Nan::FunctionCallbackInfo<v8::Value>& args);
    v8::Local<v8::Function> cb;

    //static Nan::Persistent<Function> constructor;
    QueryUSBResults usb_info();
  };

  struct AsyncWork {
    uv_async_t async;
    std::vector<string> msgArr;
    Nan::Persistent<v8::Object>* obj_persistent;
    v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>> event_callback;
    v8::Persistent<v8::Promise::Resolver, v8::CopyablePersistentTraits<v8::Promise::Resolver>> resolver;
    std::vector<v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>>> handlers;
    uv_rwlock_t lock;

  };

  struct cam_event{
    int event;
    int cam;
    double complete;
    std::string title;
    std::string message;
    mat<unsigned short>* image;
    sbig* obj;
  };

  struct cam_command{
    cam_command(int cid):command(cid){}
    int command;
    int cam;
    std::vector<double> args;
  };

  
  class sbig : public Nan::ObjectWrap {
  public:
    static void init(v8::Local<v8::Object> exports);
    
    //static Nan::Persistent<FunctionTemplate> s_cts;
    static inline Nan::Persistent<v8::Function> & constructor() {
      static Nan::Persistent<v8::Function> my_constructor;
      return my_constructor;
    }
    
  private:

    explicit sbig();
    ~sbig();

    void send_status_message(v8::Isolate* isolate, const string& type, const string& message);
    //v8::Handle<node::Buffer> gen_pngtile(v8::Handle<v8::Array>& parameters);
    
    static void New(const Nan::FunctionCallbackInfo<v8::Value>& args);
    static void shutdown_func(const Nan::FunctionCallbackInfo<v8::Value>& args);
    static void initialize_func(const Nan::FunctionCallbackInfo<v8::Value>& args);
    static void start_exposure_func(const Nan::FunctionCallbackInfo<v8::Value>& args);
    static void stop_exposure_func(const Nan::FunctionCallbackInfo<v8::Value>& args);
    static void get_temp_func(const Nan::FunctionCallbackInfo<v8::Value>& args);
    static void set_temp_func(const Nan::FunctionCallbackInfo<v8::Value>& args);
    static void shutter_func(const Nan::FunctionCallbackInfo<v8::Value>& args);
    static void filter_wheel_func(const Nan::FunctionCallbackInfo<v8::Value>& args);
    static void ccd_info_func(const Nan::FunctionCallbackInfo<v8::Value>& args);
    static void monitor_func(const Nan::FunctionCallbackInfo<v8::Value>& args);

    void config_cam(v8::Local<v8::Object>& options);
    ///static void usb_info_func(const Nan::FunctionCallbackInfo<v8::Value>& args);

    /*
    class expo_thread : public qk::thread{
      
    public:
      expo_thread(sbig* _sbc):
	sbc(_sbc), running(false)
      {}
      virtual ~expo_thread(){}
      virtual bool exec();
      sbig* sbc;
      bool running;
     
    };
    */
    
    //    expo_thread expt;
    void* event;
    
  public:

    mat<unsigned short> last_image;
    void close_shutter();
    sbig_cam *pcam;
    
    // struct eventData{
    //   int event_id;
    //   double complete;
    //   std::string type, message, id, error_message;
    //   //Nan::Persistent<v8::Function>* cb_persistent;
    //   Nan::Persistent<v8::Promise::Resolver>* persistent;

    //   Nan::Persistent<v8::Function>* emit;
    //   Nan::Persistent<v8::Object>* obj_persistent;

    // };

    // eventData edata;

    AsyncWork AW;

    std::queue<cam_command*> command_queue;
    std::queue<cam_event*> event_queue;
    
    std::queue<int> produced_nums;
    std::thread* T;
    std::mutex m;
    std::condition_variable cond_var;
    bool done = false;
    bool notified = false;

    void exec();
    void kill_thread();
    //    cond new_event;

    //    static Nan::Persistent<Function> constructor;
    //  private:
    //    v8::Local<v8::Function> cb;
    
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
    //    void really_take_exposure();
    void shutdown();
    
    bool infinite_loop;

    float exptime;
    int nexpo;
    bool light_frame=true;

    int ccd_width, ccd_height, width, height;
    std::string camera_type_string;
    double ambient_temperature;
    double ccd_temperature;
    vec<double> ccd_pixel_size;
    double ccd_gain;
    double ccd_temperature_setpoint;
    double ccd_tempreg_power;
    int continue_expo;
    std::mutex continue_expo_mut;
    //cond last_image_ready_cond;
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
