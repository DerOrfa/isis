// Generated by Bisonc++ V4.13.01 on Wed, 09 Nov 2016 17:00:23 +0100

#pragma once

#include <isis/core/propmap.hpp>
#include "VistaScanner.h"
#include "VistaParserbase.h"

// $insert namespace-open
namespace vista_internal
{

#undef VistaParser
class VistaParser: public VistaParserBase
{
    // $insert scannerobject
    VistaScanner d_scanner;
	std::list<isis::util::PropertyMap> &ch_list;
	isis::util::PropertyMap &root;
        
    public:
        int parse();
		VistaParser(std::istream &istream,isis::util::PropertyMap &global_map,std::list<isis::util::PropertyMap> &chunk_list):root(global_map),ch_list(chunk_list){
			d_scanner.switchStreams(istream);
		}


    private:
        void error(char const *msg);    // called on (syntax) errors
        int lex();                      // returns the next token from the
                                        // lexical scanner. 
        void print();                   // use, e.g., d_token, d_loc

    // support functions for parse():
        void executeAction(int ruleNr);
        void errorRecovery();
        int lookup(bool recovery);
        void nextToken();
        void print__();
        void exceptionHandler__(std::exception const &exc);
};

// $insert namespace-close
}


