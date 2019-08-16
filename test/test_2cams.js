//var fits = require('../../node-fits/build/Release/fits');
var fits = require('../node_modules/node-fits/build/Release/fits');
var sbig = require('../build/Release/sbig');

console.log("Loading driver...");


var cam1= new sbig.cam();


//var cam2= new sbig.cam();

console.log("Inspecting USB for cameras...");

sbig.usb_info(function(data){

    
    console.log("USB Info :" + JSON.stringify(data,null,4));
    

    if(data.length==0){
	console.log("No SBIG camera found!");
	return;
    }

    //Select a camera to use (First by default)


    function take_image(cam, name, cb){

//	cam.set_temp(true, -10.0); //Setting temperature regulation 
	
//	console.log("Cam cooling info = " + JSON.stringify(cam.get_temp()));

	var cam_options = {

	    exptime : 10.0,
	    nexpo : 2,
	    subframe : [12, 34, 123, 456],
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
	}).then(function(x){
	    console.log("JS: Expo Complete : " + JSON.stringify(x));
	    cb();
	}).catch(function(e){
	    console.log("JS: Expo Error : " + JSON.stringify(e));
	});
    }
    
    if(data.length>0){
	console.log("Init CAM 1 ...");
	cam1.initialize(data[0].id).then(function (init_message){
	    console.log("CAM1 OK : " + JSON.stringify(init_message));
		//console.log("CCD INFO : " + JSON.stringify(cam1.ccd_info(),null,5));
	   
	    take_image(cam1,"cam1",function(){
		try{
		    cam1.filter_wheel(1);
		}
		catch( e){
		    console.log("Cannot move any filter wheel " + e);
		}

		cam1.shutdown(function(){});
	    });
	    
	}).catch(function (error){
	    console.log("CAM1 ERROR : " + JSON.stringify(error));
	});
    }

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
    
    var i=0;
    var iv=setInterval(function(){
	console.log("Test2Cams Done ! " + i);
	if(i++>9){
	    clearInterval(iv);
	    delete cam1;
	}
	    
    },1000);

    console.log("End OF script USBINFO!");
});

console.log("End OF script!");


