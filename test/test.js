var fits = require('../node_modules/node-fits/build/Release/fits');
var sbig = require('../build/Release/sbig');

//var cam= new sbig.cam();

var fs=require("fs");




function check_jsmat(){

    var m1=new fits.mat_ushort();
    var m2=new fits.mat_ushort();

    m1.resize(10,10);
    m2.resize(10,10);

    m1.set_all(1);
    m2.set_all(16);

    m1.add(m2);
    
    var ab=m1.get_data();
    var uv = new Uint16Array(ab);
    var view1 = new DataView(ab.buffer);
    
    for(var k=0;k<20;k++){
	console.log(k + "\tcheck 17 ?\t" + m1.get_value(k) + "\tu16array  " + uv[k] +  "\tarraybuffer " + ab[k] + "\tview " + view1.getUint16(k));
    }
    
}

function check_jsmat_dbl(){

    var m1=new fits.mat_double();
    var m2=new fits.mat_double();


    m1.resize(10,10);
    m2.resize(10,10);

    m1.set_all(1.1);
    m2.set_all(16.22);

    m1.add(m2);
    
    var ab=m1.get_data();
    
    var uv = new Float64Array(ab);

    var view1 = new DataView(ab.buffer);
    
    for(var k=0;k<20;k++){
	console.log(k + " check 17.33 ? " + m1.get_value(k) + "F64Array  " + uv[k] +  " arraybuffer " + ab[k] + " view " + view1.getFloat64(k));
    }
    
}

//check_jsmat();
//check_jsmat_dbl();
//return;


function check_fits(){
    var f = new fits.file("test_1.fits"); 

    f.read_image_hdu(function(error, image){
	var ab=image.get_data();
	console.log("Image [" + image.width() + ", " +  image.height()+ " ] number of bytes " + ab.length);
	
	var uv = new Uint16Array(ab);
	
        console.log("First pix is " + uv[0]);
	
	for(var k=0;k<20;k++){
	    console.log(k + " U16: " + uv[k]);
	}
	
    });
}


//Retrieving information about the SBIG cameras connected to the USB

sbig.usb_info(function(data){

    console.log("USB Info :" + JSON.stringify(data,null,4));
    

    if(data.length==0){
	console.log("No SBIG camera found!");
	return;
    }

    //Select a camera to use (First by default)
    var cam_device=data[0].id;

    var cam= new sbig.cam();
    //Connect to the selected camera
    cam.initialize(cam_device,function (init_message){
	
	//console.log("Init : " + JSON.stringify(init_message));
	
	if(init_message.type=="success") {
	    
	    console.log(init_message.content + " --> starting exposure.");
	    
	    var expo_counter=0;
	    
	    cam.exptime=.03;
	    cam.nexpo=1;

	    cam.set_temp(1, -10.0);
	    
	    console.log("Cam cooling info = " + JSON.stringify(cam.get_temp()));
	    
	    cam.start_exposure(function (expo_message){
		
		console.log("EXPO msg : " + JSON.stringify(expo_message));
		
		if(expo_message.started){
		    return console.log(expo_message.started);
		}
		
		if(expo_message.type=="new_image"){
		    expo_counter++;
		    
		    //var img=expo_message.content;
		    var img=cam.last_image; //expo_message.content;
		    
		    
		    var fifi=new fits.file; //("test_"+(expo_counter++)+".fits");
		    fifi.file_name="test_"+(expo_counter)+".fits";
		    
		    
		    
		    console.log("New image captured,  w= " + img.width() + " h= " + img.height() + " type " + (typeof img) + ", writing in file " + fifi.file_name );
		    
		    
		    console.log("Image first values : ");
		    
		    for(var k=0;k<20;k++){
			//console.log(k+" :X");
			try{
			    var v=  img.get_value(k);
			    
			    
			    console.log(k + " : " + v);
			}
			catch (e) {
			    console.log("Exception ! " + e);
			}
		    }
		    
		    console.log("DONE: Image first values.");
		    
		    fifi.write_image_hdu(img);
		    
		    
		    var colormap=[ [0,0,0,1,0], [1,0,1,1,.8], [1,.2,.2,1,.9], [1,1,1,1,1] ];
		    var cuts=[20,50500];
		    
		    img.set_colormap(colormap);
		    img.set_cuts(cuts);
		    
		    // var out = fs.createWriteStream("big_"+expo_counter+".jpeg");
		    // out.write(img.tile( { tile_coord :  [0,0], zoom :  0, tile_size : [512,512], type : "jpeg" }));
		    // out.end();
		    
		    /*
		      fifi.set_header_key({ key : "BZERO" , value: 0}, function(r){
		      if(r!==undefined)
		      console.log("Set hdu key error : " + JSON.stringify(r));
		      });
		    */
		    

		    
		    
		    
		    // var fifi_float=new fits.file; //("test_"+(expo_counter++)+".fits");
		    // fifi_float.file_name="test_"+(expo_counter)+"_float.fits";
		    // fifi_float.write_image_hdu(cam.last_image_float);
		    
		    // console.log("Wrote FITS !");
		    
		    // var idata=cam.last_image_float.get_data();
		    // var L=idata.length/4;
		    // console.log("Image float data : T "+ typeof idata +" L " + idata.length + " 0= [" + idata.readFloatLE(0) + "]");
		    // console.log("Image float data : T "+ typeof idata +" L " + idata.length + " 1= [" + idata.readFloatLE(4) + "]");
		    // console.log("Image float data : T "+ typeof idata +" L " + idata.length + " last= [" + idata.readFloatLE(idata.length-4) + "]");
		    
		    // //if( m instanceof sbig.mat_ushort){
		}
		
		if(expo_message.type=="success"){
		    
		    //Trying temperature regulation ...
		    
		    function shutdown(){
			cam.shutdown(function (shut_msg){
	    		    if(shut_msg.off) console.log(shut_msg.off + " Ciao !");
			    
			});
		    }

		    console.log("SBIG: Start Exposure: SUCCESS: Expo counter = " + expo_counter + " N="+cam.nexpo);
		    console.log("Cam temperature = " + JSON.stringify(cam.get_temp()));
		    
		    if(expo_counter==cam.nexpo)
			shutdown();

		    
		    
		    //check_fits();
		    return;
		    
		    cam.set_temp(1, -1.0);
		    
		    // var ns=0;
		    // var iv=setInterval(function(){
		    // 	console.log("NS= "+ ns +" Cam temperature = " + JSON.stringify(cam.get_temp()));
		    // 	if(ns++>2){
			    
		    // 	    clearInterval(iv);
			    
		    // 	    console.log(expo_message.done + " --> cam shutdown ...");
		    // 	    shutdown();
		    // 	}
			
		    // }, 1000);
		    
		    
		}
		
		if(expo_message.type=="error"){		
		    console.log("SBIG: Start Exposure : Error : " + expo_message.content + "");
		}
		
	    });
	    
	    //console.log("After start exposure...");
	    
	    
	    
	    
	}
	
	if(init_message.type=="info") {
	    console.log("SBIG:Initialize Info : " + init_message.content );
	};
	
	if(init_message.type=="error") {
	    console.log("SBIG:Initialize Error : " + init_message.content );
	}
    });
    
});



