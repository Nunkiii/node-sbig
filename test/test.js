var fits = require('../node-fits/build/Release/fits');
var sbig = require('./build/Release/sbig');

var cam= new sbig.cam();

cam.initialize(function (init_message){

    if(init_message.ready) {

	console.log(init_message.ready + " --> starting exposure.");

	var expo_counter=0;

	cam.exptime=.05;

	cam.start_exposure(function (expo_message){
	    
	    if(expo_message.started){
		return console.log(expo_message.started);
	    }
	    
	    if(expo_message.new_image){

		var img=expo_message.new_image;
		var fifi=new fits.file("test_"+(expo_counter++)+".fits");

		console.log("New image ! w= " + img.width() + " h= " + img.height() + " writing in file " + fifi.file_name );

		fifi.write_image_hdu(img);

		//if( m instanceof sbig.mat_ushort){
	    }

	    if(expo_message.done){
		console.log(expo_message.done + " --> cam shutdown ...");
		
		cam.shutdown(function (shut_msg){
	    	    if(shut_msg.off) console.log(shut_msg.off + " Ciao !");
		    
		});
	    }
	    
	    if(expo_message.error){		
		console.log(expo_message.error + "");
	    }
	    
	});
	
	console.log("After start exposure...");
    }
});


console.log("END of test.js");