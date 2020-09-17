/* MyMEXFunction
 * Adds second input to each
 * element of first input
 * a = MyMEXFunction(a,b);
*/

#include "mexAdapter.hpp"
#include "common.hpp"
#include "property_transfer.hpp"
#include "image_transfer.hpp"
#include "isis/core/io_factory.hpp"

using namespace matlab::data;
using matlab::mex::ArgumentList;
using namespace isis;

class MexFunction : public matlab::mex::Function {
public:
	MexFunction() {mat::MatMessagePrint::enginePtr=getEngine();}
	~MexFunction() override{mat::MatMessagePrint::enginePtr.reset();}
	void operator()(ArgumentList outputs, ArgumentList inputs)override {
		static ArrayFactory  f;
		ENABLE_LOG(MatlabLog,mat::MatMessagePrint,info);
		ENABLE_LOG(MatlabDebug,mat::MatMessagePrint,info);
		util::enableLog<mat::MatMessagePrint>(notice);
        data::enableLog<mat::MatMessagePrint>(notice);
        image_io::enableLog<mat::MatMessagePrint>(error);

		std::string fname;
		std::list<util::istring> formatstack,dialects;

		switch (inputs.size()) {
			case 3:
				dialects = mat::getArgumentList<util::istring>(inputs,1);
			case 2:
				formatstack = mat::getArgumentList<util::istring>(inputs,1);
			case 1:
				fname = mat::getArgument<std::string>(inputs,0);
				break;
			default:
				LOG(MatlabLog,error) << "Need at least a file, or directory name";
				return;
		}

		if(!fname.empty()) {
			auto images = data::IOFactory::load(fname,formatstack,dialects);
			LOG(MatlabLog,info) << "Loaded " << images.size() << " from " << fname;
			auto out = f.createStructArray({images.size(),1},{"metadata","data"});
			size_t i = 0;
			while(!images.empty()) {
				data::Image &img=images.front();
                out[i]["metadata"] = mat::branchToStruct(img);
				out[i]["data"] = mat::imageToArray(img);
				images.pop_front();
				++i;
			}
			outputs[0]=out;
		}
	}

};
