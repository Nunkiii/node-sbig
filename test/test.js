var fits = require('../../node-fits/build/Release/fits');
var sbig = require('../build/Release/sbig');

var cam= new sbig.cam();

var fs=require("fs");

cam.initialize(function (init_message){

    console.log("Init : " + JSON.stringify(init_message));
    
    if(init_message.type=="success") {

	console.log(init_message.content + " --> starting exposure.");

	var expo_counter=0;

	cam.exptime=.5
	cam.nexpo=1;

	console.log("Cam cooling info = " + JSON.stringify(cam.get_temp()));

	cam.start_exposure(function (expo_message){

	    console.log("EXPO msg : " + JSON.stringify(expo_message));
	    
	    if(expo_message.started){
		return console.log(expo_message.started);
	    }
	    
	    if(expo_message.type=="new_image"){

		var img=expo_message.content;
		var fifi=new fits.file; //("test_"+(expo_counter++)+".fits");
		fifi.file_name="test_"+(expo_counter++)+".fits";
		


		console.log("New image ! w= " + img.width() + " h= " + img.height() + " writing in file " + fifi.file_name );


		

		
		var out = fs.createWriteStream("big.jpeg");
		//out.write(img.tile( { tile_coord :  [0,0], zoom :  0, tile_size : [256,256], type : "jpeg" }));
		out.end();
		
		fifi.write_image_hdu(img);

		fifi.set_header_key({ key : "BZERO" , value: 0}, function(r){
		    if(r!==undefined)
			console.log("Set hdu key error : " + JSON.stringify(r));
		});
		
		
		var fifi_float=new fits.file; //("test_"+(expo_counter++)+".fits");
		fifi_float.file_name="test_"+(expo_counter++)+"_float.fits";
		fifi_float.write_image_hdu(cam.last_image_float);

		console.log("Wrote FITS !");

		var idata=cam.last_image_float.get_data();
		var L=idata.length/4;
		console.log("Image float data : T "+ typeof idata +" L " + idata.length + " 0= [" + idata.readFloatLE(0) + "]");
		console.log("Image float data : T "+ typeof idata +" L " + idata.length + " 1= [" + idata.readFloatLE(4) + "]");
		console.log("Image float data : T "+ typeof idata +" L " + idata.length + " last= [" + idata.readFloatLE(idata.length-4) + "]");
		
		//if( m instanceof sbig.mat_ushort){
	    }

	    if(expo_message.type=="success"){

		//Trying temperature regulation ...

		cam.set_temp(1, -1.0);

		var ns=0;
		var iv=setInterval(function(){
		    console.log("NS= "+ ns +" Cam temperature = " + JSON.stringify(cam.get_temp()));
		    if(ns++>2){
			
			clearInterval(iv);

			console.log(expo_message.done + " --> cam shutdown ...");
			cam.shutdown(function (shut_msg){
	    		    if(shut_msg.off) console.log(shut_msg.off + " Ciao !");
			    
			});
			
		    }
		    
		}, 1000);
		

	    }
	    
	    if(expo_message.type=="error"){		
		console.log("Error : " + expo_message.content + "");
	    }
	    
	});
	
	//console.log("After start exposure...");
	



    }else
	console.log("Camera init error : " + init_message.content );
});

//console.log("END of test.js");
