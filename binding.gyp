{
    "targets": [{

        "target_name": "sbig",

        "sources": [            
	    "src/qk/exception.cpp",
            "src/qk/pngwriter.cpp",
            "src/qk/jpeg_writer.cpp",
            "src/qk/threads.cpp",
            "src/csbigcam/csbigcam.cpp",
            "src/csbigcam/csbigimg.cpp",
            "src/sbigcam.cpp"
        ],

        "conditions": [

            ["OS=='linux'", {
                "libraries": ["-fPIC",
                              "-lpng",
                              "-ljpeg",
			      "-lsbigudrv",
                              "/usr/lib/x86_64-linux-gnu/libusb-1.0.so"			      
			      "-lcfitsio"
                             ],
                "cflags_cc": ["-fexceptions",
                              "-I /usr/include/libusb-1.0",
                              "-I../node_modules/node-fits",
                              "-frtti",
                              "-I.", "-I.."
                             ],
                "cflags_cc!": [
                    "-fno-exceptions"
                ]
            }]
        ],
       "include_dirs": ["<!(node -e 'require(\"nan\")')", "src/"],

    }],
    "dependencies": {
        "nan": "*"
    }
}
