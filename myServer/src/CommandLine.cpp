#include <CommandLine.hpp>
#include <boost/program_options.hpp>
#include <string.h>
#include <iostream>
#include <iterator>


namespace po = boost::program_options;


CommandLine::CommandLine(int argc,  char** argv) {
	try {
		po::options_description desc("Allowed options");
		desc.add_options()
		    ("help", "produce help message")
		    ("radio", po::value<std::string>(), "set radio type: use \"lime\" or \"hackrf\"")
		    ("audio", po::value<std::string>(), "set audio type: use \"wav\" or \"alsa\"")
            ("server", po::value<std::string>(), "set server on or off: use \"on\" or \"off\"");;
		;
		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);  

		if (vm.count("help")) {
            std::cout << desc << "\n";
            return;
        }

        if (vm.count("radio")) {
            if (vm["radio"].as<std::string>() == "lime") {
            	radio = lime;
            }
            if (vm["radio"].as<std::string>() == "hackrf") {
            	radio = hackrf;
            }
        } 

        if (vm.count("audio")) {
            if (vm["audio"].as<std::string>() == "wav") {
            	audio = wav;
            }
            if (vm["audio"].as<std::string>() == "alsa") {
            	audio = alsa;
            }
        } 
        if (vm.count("server")) {
            if (vm["server"].as<std::string>() == "on") {
                server = on;
            }
            if (vm["server"].as<std::string>() == "off") {
                server = off;
            }
        } 
	}
	catch(...) {
        std::cout << "Exception parsing command line. Use --help for instructions on options.\n";
    }

}