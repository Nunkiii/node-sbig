{
    'targets': [
	{
	    "target_name": "sbig",	
	    "sources": [ 
		"../node-fits/colormap/colormap_interface.cpp", 
		"../node-fits/fits/fits.cpp", 
		"../node-fits/qk/exception.cpp", 
		"../node-fits/qk/pngwriter.cpp", 
		"../node-fits/qk/threads.cpp", 
		"src/csbigcam/csbigcam.cpp",
		"src/csbigcam/csbigimg.cpp", 
		"src/sbigcam.cpp", 
	    ],
	    
	    'conditions': [
		['OS=="linux"', {
		    'ldflags': ['-lcfitsio','-lpng','-lsbigudrv'],
		    'cflags_cc': [ '-fexceptions','-I../../node-fits', '-frtti', '-I.', '-I..'],
		    'cflags_cc!': [
			'-fno-exceptions'
		    ],
		}],
            ],
	    
	},
    ],
  }

