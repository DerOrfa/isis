/* MyMEXFunction
 * Adds second input to each
 * element of first input
 * a = MyMEXFunction(a,b);
*/

#include "mexAdapter.hpp"
#include "common.hpp"
#include "property_transfer.hpp"
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
		ENABLE_LOG(MatlabLog,mat::MatMessagePrint,notice);
		ENABLE_LOG(MatlabDebug,mat::MatMessagePrint,notice);

		auto fname = mat::getArgument<std::string>(inputs,0);

		LOG(CoreLog,info) << fname;

		if(!fname.empty()) {
			auto images = data::IOFactory::load(fname);
			auto out = f.createStructArray({images.size(),1},{"metadata"});
			size_t i = 0;
			while(!images.empty()) {
				out[i++]["metadata"] = mat::branchToStruct(images.front());
				images.pop_front();
			}
			outputs[0]=out;
		}
	}

};
