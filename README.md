A Node.js frontend to the SBIG astronomical cameras.
============

This C++/javascript library is in early developement phase. 

### To build:

The `node-sbig` module compiles on GNU/Linux with g++. `node-gyp` is used as building tool. Other platforms have not been tested yet.

You will need to install the development versions of `libusb-1.0-0`, `libpng` and `libcfitsio` (called respectively `libusb-1.0-0-dev`, `libpng-dev` and `libcfitsio-dev` on debian systems).

The `libsbigudrv.so` SBIG proprietary library driver and the camera firmware must be correctly installed on your system and the camera clearly identified (`lsusb` to check). We suggest to use the `libsbig` package provided by `indi-sbig` in the version provided by the repositories. 

On Debian-like systems to install node (for example):

    sudo apt install node node-gyp 
    
And to install dependecies:
    
    sudo apt install g++ libpng-dev libcfitsio-dev libusb-1.0-0-dev libsbig
   
In the node-sbig directory, build the module. This will also install our [node-fits](https://github.com/Nunkiii/node-fits) package from github:

    npm -f install
    
### Testing

The test file, in the node-sbig/test directory 

    $node test_2cams.js

```
var fits = require('../node_modules/node-fits/build/Release/fits');
var sbig = require('../build/Release/sbig');

console.log("Loading driver...");

var cam1= new sbig.cam();
var cam2= new sbig.cam();

console.log("Inspecting USB for cameras...");

sbig.usb_info(function(data){

    
    console.log("USB Info :" + JSON.stringify(data,null,4));
    

    if(data.length==0){
	console.log("No SBIG camera found!");
	return;
    }

    //Select a camera to use (First by default)


    function take_image(cam, name){

	cam.set_temp(true, -10.0); //Setting temperature regulation 
	
	console.log("Cam cooling info = " + JSON.stringify(cam.get_temp()));

	var cam_options = {

	    exptime : 0.3,
	    nexpo : 1,
	    //subframe : [0, 0, 100, 100],
	    fast_readout : false,
	    dual_channel : false,
	    light_frame: true,
	    readout_mode: "2x2"
	};
	
	cam.start_exposure(cam_options, function (expo_message){
	    
	    console.log("EXPO msg : " + JSON.stringify(expo_message));
	    
	    if(expo_message.started){
		return console.log(expo_message.started);
	    }
	    
	    if(expo_message.type=="new_image"){
		//var img=expo_message.content;  BUG HERE!
		var img=cam.last_image; //tbr...

		
		var fifi=new fits.file;
		fifi.file_name=name+".fits";
		
		console.log("New image captured,  w= " + img.width() + " h= " + img.height() + " type " + (typeof img) + ", writing FITS file " + fifi.file_name );
		
		fifi.write_image_hdu(img);
	    }
	});
    }
    
    if(data.length>0)
	cam1.initialize(data[0].id,function (init_message){
	    console.log("CAM1 Init : " + JSON.stringify(init_message));
	    
	    if(init_message.type=="success") {
		
		console.log(init_message.content + " --> starting exposure.");
		take_image(cam1,"cam1");

		cam1.filter_wheel(1);
	    }
	    if(init_message.type=="info") {
		console.log("Cam1 Info : " + init_message.content );
	    };
	    
	    if(init_message.type=="error") {
		console.log("Cam1 Error : " + init_message.content );
	    }
	});

    if(data.length>1)
	cam2.initialize(data[1].id,function (init_message){
	    console.log("CAM2 Init : " + JSON.stringify(init_message));
	    if(init_message.type=="success") {
		
		console.log(init_message.content + " --> starting exposure.");
		take_image(cam2,"cam2");
	    }
	    if(init_message.type=="info") {
		console.log("Cam2 Info : " + init_message.content );
	    };
	    
	    if(init_message.type=="error") {
		console.log("Cam2 Error : " + init_message.content );
	    }
	    
	    //take_image(cam2,"cam2");
	});
      
    
});





















```
