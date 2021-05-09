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
		    ("audio", po::value<std::string>(), "set audio type: use \"wav\" or \"alsa\"");
		;
		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);  

		if (vm.count("help")) {
            cout << desc << "\n";
            return 0;
        }

        if (vm.count("radio")) {
            if (vm["radio"].as<std::string>() == "lime") {
            	radio = lime
            }
            if (vm["radio"].as<std::string>() == "hackrf") {
            	radio = hackrf
            }
        } 

        if (vm.count("audio")) {
            if (vm["radio"].as<std::string>() == "wav") {
            	audio = wav
            }
            if (vm["radio"].as<std::string>() == "alsa") {
            	audio = alsa
            }
        } 
	}
	catch(...) {
        std::cout << "Exception parsing command line. Use --help for instructions on options.\n";
    }

}