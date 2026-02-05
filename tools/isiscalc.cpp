#include <isis/core/io_application.hpp>
#include <isis/core/io_factory.hpp>
#include <muParser.h>


using namespace isis;

class VoxelOp
{
	mu::Parser parser;
	double voxBuff;
	util::dvector4 posBuff;
	void init() {
		parser.DefineVar( std::string( "vox" ), &voxBuff );
		parser.DefineVar( std::string( "pos_x" ), &posBuff[data::rowDim] );
		parser.DefineVar( std::string( "pos_y" ), &posBuff[data::columnDim] );
		parser.DefineVar( std::string( "pos_z" ), &posBuff[data::sliceDim] );
		parser.DefineVar( std::string( "pos_t" ), &posBuff[data::timeDim] );
	}
public:
	VoxelOp(const VoxelOp &other):parser(other.parser) {
		init();
	}
	VoxelOp( std::string expr ) {
		parser.SetExpr( expr );
		init();
	}
	bool operator()( double &vox, const isis::util::vector4<size_t>& pos ) {
		voxBuff = vox; //using parser.DefineVar every time would slow down the evaluation
		posBuff = {double(pos[0]),double(pos[1]),double(pos[2])};
		vox = parser.Eval();
		return true;
	}

};

int main( int argc, char **argv )
{
	data::IOApplication app( "isis calc", true, true );
	app.parameters["voxelop"] = std::string( "vox" );
	app.parameters["voxelop"].setDescription( "a term to evaluate the new value of each voxel. Available variables are: vox,pos_x,pos_y,pos_z,pos_t." );
	app.init( argc, argv, true ); // will exit if there is a problem


	const std::string op = app.parameters["voxelop"];
	std::list<data::Image> out_images;

	try {
		VoxelOp vop( op );
		while (!app.images.empty()) { //muparser needs double
			auto img = app.fetchImageAs<double>();
			std::cout << "Computing vox=(" << op << ") for each voxel of the " << img.getSizeAsString() << "-Image" << std::endl;
			img.foreachVoxel( vop );
			out_images.push_back(std::move(img));
		}
	} catch( mu::Parser::exception_type &e ) {
		std::cerr << e.GetMsg() << std::endl;
		exit( -1 );
	}

	app.autowrite( out_images );
	return EXIT_SUCCESS;
}
