{
    "targets": [{
        "target_name": "sbig",
        "sources": [
            
	    "./node_modules/node-fits/qk/exception.cpp",
            "./node_modules/node-fits/qk/pngwriter.cpp",
            "./node_modules/node-fits/qk/jpeg_writer.cpp",
            "./node_modules/node-fits/qk/threads.cpp",
            "src/csbigcam/csbigcam.cpp",
            "src/csbigcam/csbigimg.cpp",
            "src/sbigcam.cpp"
        ],
        "conditions": [
            ["OS=='linux'", {
                "ldflags": [],
                "libraries": ["-fPIC",
                              "-lcfitsio",
                              "-lpng",
                              "-ljpeg",
                              "-lsbig",
                              "/usr/lib/x86_64-linux-gnu/libusb-1.0.so"
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
        "include_dirs": ["<!(node -e \"require('nan')\")"]
    }],
    "dependencies": {
        "nan": "^2.7.0",
        "node-fits": "github:Nunkiii/node-fits"
    }
}
