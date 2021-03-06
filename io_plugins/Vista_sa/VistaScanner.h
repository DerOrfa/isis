// Generated by Flexc++ V2.03.04 on Wed, 09 Nov 2016 17:02:09 +0100

#pragma once

#include <isis/core/propmap.hpp>
#include "VistaScannerbase.h"
#include "VistaParserbase.h"

// $insert namespace-open
namespace vista_internal
{

// $insert classHead
class VistaScanner: public VistaScannerBase
{
    public:
        explicit VistaScanner(std::istream &in = std::cin,
                                std::ostream &out = std::cout);

        VistaScanner(std::string const &infile, std::string const &outfile);
        
        // $insert lexFunctionDecl
        int lex();

    private:
        int lex__();
        int executeAction__(size_t ruleNr);

        void print();
        void preCode();     // re-implement this function for code that must 
                            // be exec'ed before the patternmatching starts

        void postCode(PostEnum__ type);    
                            // re-implement this function for code that must 
                            // be exec'ed after the rules's actions.
};

// $insert scannerConstructors
inline VistaScanner::VistaScanner(std::istream &in, std::ostream &out)
:
    VistaScannerBase(in, out)
{}

inline VistaScanner::VistaScanner(std::string const &infile, std::string const &outfile)
:
    VistaScannerBase(infile, outfile)
{}

// $insert inlineLexFunction
inline int VistaScanner::lex()
{
    return lex__();
}

inline void VistaScanner::preCode() 
{
    // optionally replace by your own code
}

inline void VistaScanner::postCode(PostEnum__ type) 
{
    // optionally replace by your own code
}

inline void VistaScanner::print() 
{
    print__();
}

// $insert namespace-close
}



