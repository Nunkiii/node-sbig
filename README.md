A Node.js frontend to the SBIG astronomical cameras.
============

This C++/javascript library is in early developement phase. 

### To build:

The `node-sbig` module compiles on GNU/Linux with g++. `node-gyp` is used as building tool. Other platforms have not been tested yet.

You will need to install the development versions of `libpng` and `libcfitsio` (called respectively `libpng12-dev` and `libcfitsio3-dev` on debian systems). 

On Debian-like systems,

    #apt-get install node node-gyp g++ libpng-dev libcfitsio3-dev


The `libsbigudrv.so` SBIG proprietary library driver and the camera firmware must be correctly installed on your system and the camera clearly identified (`lsusb` to check). 

   
You will need the `node-fits` plugin source code on the same directory as this module for the compilation to succeed. In the node-sbig directory, the following commands should then build the module:


    $node-gyp configure
    $node-gyp build
    

### Testing

The test.js file, in the node-sbig/test directory 

    $node test.js
    


```

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

```
