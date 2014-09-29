//---------------------------------------------------------------------------//
/*!
 * \file tstSharedDomainMap8.cpp
 * \author Stuart R. Slattery
 * \brief Shared domain map unit test 8 for 3D hybrid meshes.
 */
//---------------------------------------------------------------------------//

#include <iostream>
#include <vector>
#include <cmath>
#include <sstream>
#include <algorithm>
#include <cassert>
#include <ctime>
#include <cstdlib>

#include <DTK_SharedDomainMap.hpp>
#include <DTK_FieldTraits.hpp>
#include <DTK_FieldEvaluator.hpp>
#include <DTK_MeshTypes.hpp>
#include <DTK_MeshBlock.hpp>
#include <DTK_MeshContainer.hpp>

#include <Teuchos_UnitTestHarness.hpp>
#include <Teuchos_DefaultComm.hpp>
#include <Teuchos_CommHelpers.hpp>
#include <Teuchos_RCP.hpp>
#include <Teuchos_ArrayRCP.hpp>
#include <Teuchos_OpaqueWrapper.hpp>
#include <Teuchos_Array.hpp>
#include <Teuchos_TypeTraits.hpp>

//---------------------------------------------------------------------------//
// MPI Setup
//---------------------------------------------------------------------------//

template<class Ordinal>
Teuchos::RCP<const Teuchos::Comm<Ordinal> > getDefaultComm()
{
#ifdef HAVE_MPI
    return Teuchos::DefaultComm<Ordinal>::getComm();
#else
    return Teuchos::rcp(new Teuchos::SerialComm<Ordinal>() );
#endif
}

//---------------------------------------------------------------------------//
// Field implementation.
//---------------------------------------------------------------------------//
class MyField
{
  public:

    typedef double value_type;
    typedef Teuchos::Array<double>::size_type size_type;
    typedef Teuchos::Array<double>::iterator iterator;
    typedef Teuchos::Array<double>::const_iterator const_iterator;

    MyField( size_type size, int dim )
	: d_dim( dim )
	, d_data( dim*size )
    { /* ... */ }

    ~MyField()
    { /* ... */ }

    int dim() const
    { return d_dim; }

    size_type size() const
    { return d_data.size(); }

    bool empty() const
    { return d_data.empty(); }

    iterator begin()
    { return d_data.begin(); }

    const_iterator begin() const
    { return d_data.begin(); }

    iterator end()
    { return d_data.end(); }

    const_iterator end() const
    { return d_data.end(); }

    Teuchos::Array<double>& getData()
    { return d_data; }

    const Teuchos::Array<double>& getData() const
    { return d_data; }

  private:
    int d_dim;
    Teuchos::Array<double> d_data;
};

//---------------------------------------------------------------------------//
// DTK implementations.
//---------------------------------------------------------------------------//
namespace DataTransferKit
{

//---------------------------------------------------------------------------//
// Field Traits specification for MyField
template<>
class FieldTraits<MyField>
{
  public:

    typedef MyField                    field_type;
    typedef double                     value_type;
    typedef MyField::size_type         size_type;
    typedef MyField::iterator          iterator;
    typedef MyField::const_iterator    const_iterator;

    static inline size_type dim( const MyField& field )
    { return field.dim(); }

    static inline size_type size( const MyField& field )
    { return field.size(); }

    static inline bool empty( const MyField& field )
    { return field.empty(); }

    static inline iterator begin( MyField& field )
    { return field.begin(); }

    static inline const_iterator begin( const MyField& field )
    { return field.begin(); }

    static inline iterator end( MyField& field )
    { return field.end(); }

    static inline const_iterator end( const MyField& field )
    { return field.end(); }
};

} // end namespace DataTransferKit

//---------------------------------------------------------------------------//
// FieldEvaluator Implementation.
class MyEvaluator : 
    public DataTransferKit::FieldEvaluator<DataTransferKit::MeshId,MyField>
{
  public:

    MyEvaluator( const Teuchos::RCP<DataTransferKit::MeshBlock>& mesh, 
		 const Teuchos::RCP< const Teuchos::Comm<int> >& comm )
	: d_mesh( mesh )
	, d_comm( comm )
    { /* ... */ }

    ~MyEvaluator()
    { /* ... */ }

    MyField evaluate( 
	const Teuchos::ArrayRCP<DataTransferKit::MeshId>& elements,
	const Teuchos::ArrayRCP<double>& coords )
    {
	MyField evaluated_data( elements.size(), 1 );
	for ( int n = 0; n < elements.size(); ++n )
	{
	    if ( std::find( d_mesh->elementIds().begin(),
			    d_mesh->elementIds().end(),
			    elements[n] ) != d_mesh->elementIds().end() )
	    {
		*(evaluated_data.begin() + n ) = d_comm->getRank() + 1.0;
	    }
	    else
	    {
 		*(evaluated_data.begin() + n ) = 0.0;
	    }
	}
	return evaluated_data;
    }

  private:

    Teuchos::RCP<DataTransferKit::MeshBlock> d_mesh;
    Teuchos::RCP< const Teuchos::Comm<int> > d_comm;
};

//---------------------------------------------------------------------------//
// Mesh create function.
//---------------------------------------------------------------------------//
Teuchos::RCP<DataTransferKit::MeshBlock> 
buildMyMesh( int my_rank, int my_size, int edge_length )
{
    // Make some vertices.
    int num_vertices = edge_length*edge_length*2;
    int vertex_dim = 3;
    Teuchos::ArrayRCP<DataTransferKit::MeshId> vertex_handles( num_vertices );
    Teuchos::ArrayRCP<double> coords( vertex_dim*num_vertices );
    int idx;
    for ( int j = 0; j < edge_length; ++j )
    {
	for ( int i = 0; i < edge_length; ++i )
	{
	    idx = i + j*edge_length;
	    vertex_handles[ idx ] = (DataTransferKit::MeshId) num_vertices*my_rank + idx;
	    coords[ idx ] = i + my_rank*(edge_length-1);
	    coords[ num_vertices + idx ] = j;
	    coords[ 2*num_vertices + idx ] = 0.0;
	}
    }
    for ( int j = 0; j < edge_length; ++j )
    {
	for ( int i = 0; i < edge_length; ++i )
	{
	    idx = i + j*edge_length + edge_length*edge_length;
	    vertex_handles[ idx ] = (DataTransferKit::MeshId) num_vertices*my_rank + idx;
	    coords[ idx ] = i + my_rank*(edge_length-1);
	    coords[ num_vertices + idx ] = j;
	    coords[ 2*num_vertices + idx ] = 1.0;
	}
    }
    
    // Make the wedges. 
    int num_elements = (edge_length-1)*(edge_length-1)*2;
    Teuchos::ArrayRCP<DataTransferKit::MeshId> wedge_handles( num_elements );
    Teuchos::ArrayRCP<DataTransferKit::MeshId> wedge_connectivity( 6*num_elements );
    int elem_idx, vertex_idx;
    int v0, v1, v2, v3, v4, v5, v6, v7;
    for ( int j = 0; j < (edge_length-1); ++j )
    {
	for ( int i = 0; i < (edge_length-1); ++i )
	{
	    // Indices.
	    vertex_idx = i + j*edge_length;
	    v0 = vertex_idx;
	    v1 = vertex_idx + 1;
	    v2 = vertex_idx + 1 + edge_length;
	    v3 = vertex_idx +     edge_length;
	    v4 = vertex_idx +                   edge_length*edge_length;
	    v5 = vertex_idx + 1 +               edge_length*edge_length;
	    v6 = vertex_idx + 1 + edge_length + edge_length*edge_length;
	    v7 = vertex_idx +     edge_length + edge_length*edge_length;

	    // Wedge 1.
	    elem_idx = i + j*(edge_length-1);
	    wedge_handles[elem_idx] = num_elements*my_rank + elem_idx;
	    wedge_connectivity[elem_idx]                = vertex_handles[v0];
	    wedge_connectivity[num_elements+elem_idx]   = vertex_handles[v4];
	    wedge_connectivity[2*num_elements+elem_idx] = vertex_handles[v1];
	    wedge_connectivity[3*num_elements+elem_idx] = vertex_handles[v3];
	    wedge_connectivity[4*num_elements+elem_idx] = vertex_handles[v7];
	    wedge_connectivity[5*num_elements+elem_idx] = vertex_handles[v2];

	    // Wedge 2.
	    elem_idx = i + j*(edge_length-1) + num_elements/2;
	    wedge_handles[elem_idx] = num_elements*my_rank + elem_idx;
	    wedge_connectivity[elem_idx] 	        = vertex_handles[v1];
	    wedge_connectivity[num_elements+elem_idx]   = vertex_handles[v4];
	    wedge_connectivity[2*num_elements+elem_idx] = vertex_handles[v5];
	    wedge_connectivity[3*num_elements+elem_idx] = vertex_handles[v2];
	    wedge_connectivity[4*num_elements+elem_idx] = vertex_handles[v7];
	    wedge_connectivity[5*num_elements+elem_idx] = vertex_handles[v6];
	}
    }

    Teuchos::ArrayRCP<int> permutation_list( 6 );
    for ( int i = 0; i < permutation_list.size(); ++i )
    {
	permutation_list[i] = i;
    }

    return Teuchos::rcp(
	new DataTransferKit::MeshContainer(
	    3, vertex_handles, coords, 
	    DataTransferKit::DTK_WEDGE, 6,
	    wedge_handles, wedge_connectivity,
	    permutation_list ) );
}

//---------------------------------------------------------------------------//
Teuchos::RCP<DataTransferKit::MeshBlock> 
buildTiledMesh( int my_rank, int my_size, int edge_length )
{
    // Make some vertices.
    int num_vertices = edge_length*edge_length*2;
    int vertex_dim = 3;
    Teuchos::ArrayRCP<DataTransferKit::MeshId> vertex_handles( num_vertices );
    Teuchos::ArrayRCP<double> coords( vertex_dim*num_vertices );
    int idx;
    for ( int j = 0; j < edge_length; ++j )
    {
	for ( int i = 0; i < edge_length; ++i )
	{
	    idx = i + j*edge_length;
	    vertex_handles[ idx ] = (DataTransferKit::MeshId) num_vertices*my_rank + idx;
	    coords[ idx ] = i + my_rank*(edge_length-1);
	    coords[ num_vertices + idx ] = j + my_rank*(edge_length-1);
	    coords[ 2*num_vertices + idx ] = 0.0;
	}
    }
    for ( int j = 0; j < edge_length; ++j )
    {
	for ( int i = 0; i < edge_length; ++i )
	{
	    idx = i + j*edge_length + edge_length*edge_length;
	    vertex_handles[ idx ] = (DataTransferKit::MeshId) num_vertices*my_rank + idx;
	    coords[ idx ] = i + my_rank*(edge_length-1);
	    coords[ num_vertices + idx ] = j + my_rank*(edge_length-1);
	    coords[ 2*num_vertices + idx ] = 1.0;
	}
    }
    
    // Make the wedges. 
    int num_elements = (edge_length-1)*(edge_length-1)*2;
    Teuchos::ArrayRCP<DataTransferKit::MeshId> wedge_handles( num_elements );
    Teuchos::ArrayRCP<DataTransferKit::MeshId> wedge_connectivity( 6*num_elements );
    int elem_idx, vertex_idx;
    int v0, v1, v2, v3, v4, v5, v6, v7;
    for ( int j = 0; j < (edge_length-1); ++j )
    {
	for ( int i = 0; i < (edge_length-1); ++i )
	{
	    // Indices.
	    vertex_idx = i + j*edge_length;
	    v0 = vertex_idx;
	    v1 = vertex_idx + 1;
	    v2 = vertex_idx + 1 + edge_length;
	    v3 = vertex_idx +     edge_length;
	    v4 = vertex_idx +                   edge_length*edge_length;
	    v5 = vertex_idx + 1 +               edge_length*edge_length;
	    v6 = vertex_idx + 1 + edge_length + edge_length*edge_length;
	    v7 = vertex_idx +     edge_length + edge_length*edge_length;

	    // Wedge 1.
	    elem_idx = i + j*(edge_length-1);
	    wedge_handles[elem_idx] = num_elements*my_rank + elem_idx;
	    wedge_connectivity[elem_idx]                = vertex_handles[v0];
	    wedge_connectivity[num_elements+elem_idx]   = vertex_handles[v4];
	    wedge_connectivity[2*num_elements+elem_idx] = vertex_handles[v1];
	    wedge_connectivity[3*num_elements+elem_idx] = vertex_handles[v3];
	    wedge_connectivity[4*num_elements+elem_idx] = vertex_handles[v7];
	    wedge_connectivity[5*num_elements+elem_idx] = vertex_handles[v2];

	    // Wedge 2.
	    elem_idx = i + j*(edge_length-1) + num_elements/2;
	    wedge_handles[elem_idx] = num_elements*my_rank + elem_idx;
	    wedge_connectivity[elem_idx] 	        = vertex_handles[v1];
	    wedge_connectivity[num_elements+elem_idx]   = vertex_handles[v4];
	    wedge_connectivity[2*num_elements+elem_idx] = vertex_handles[v5];
	    wedge_connectivity[3*num_elements+elem_idx] = vertex_handles[v2];
	    wedge_connectivity[4*num_elements+elem_idx] = vertex_handles[v7];
	    wedge_connectivity[5*num_elements+elem_idx] = vertex_handles[v6];
	}
    }

    Teuchos::ArrayRCP<int> permutation_list( 6 );
    for ( int i = 0; i < permutation_list.size(); ++i )
    {
	permutation_list[i] = i;
    }

    return Teuchos::rcp(
	new DataTransferKit::MeshContainer(
	    3, vertex_handles, coords, 
	    DataTransferKit::DTK_WEDGE, 6,
	    wedge_handles, wedge_connectivity,
	    permutation_list ) );
}

//---------------------------------------------------------------------------//
// Coordinate field create functions.
//---------------------------------------------------------------------------//
void buildCoordinateField( int my_rank, int my_size, 
			   int num_points, int edge_size,
			   Teuchos::RCP<MyField>& coordinate_field )
{
    std::srand( my_rank*num_points*2 );
    for ( int i = 0; i < num_points; ++i )
    {
	*(coordinate_field->begin() + i) = 
	    my_size * (edge_size-1) * (double) std::rand() / RAND_MAX;
	*(coordinate_field->begin() + num_points + i ) = 
	    (edge_size-1) * (double) std::rand() / RAND_MAX;
	*(coordinate_field->begin() + 2*num_points + i ) = 
	    (double) std::rand() / RAND_MAX;
    }
}

//---------------------------------------------------------------------------//
void buildExpandedCoordinateField( int my_rank, int my_size, 
				   int num_points, int edge_size,
				   Teuchos::RCP<MyField>& coordinate_field )
{
    std::srand( my_rank*num_points*2 );
    for ( int i = 0; i < num_points; ++i )
    {
	*(coordinate_field->begin() + i) = 
	    my_size * (edge_size) * (double) std::rand() / RAND_MAX - 0.5;
	*(coordinate_field->begin() + num_points + i ) = 
	    (edge_size) * (double) std::rand() / RAND_MAX - 0.5;
	*(coordinate_field->begin() + 2*num_points + i ) = 
	    1.2 * (double) std::rand() / RAND_MAX - 0.1;
    }
}

//---------------------------------------------------------------------------//
void buildTiledCoordinateField( int my_rank, int my_size, 
				int num_points, int edge_size,
				Teuchos::RCP<MyField>& coordinate_field )
{
    std::srand( my_rank*num_points*2 );
    for ( int i = 0; i < num_points; ++i )
    {
	*(coordinate_field->begin() + i) = 
	    my_size * (edge_size) * (double) std::rand() / RAND_MAX - 0.5;
	*(coordinate_field->begin() + num_points + i ) = 
	    my_size * (edge_size) * (double) std::rand() / RAND_MAX - 0.5;
	*(coordinate_field->begin() + 2*num_points + i ) = 
	    1.2 * (double) std::rand() / RAND_MAX - 0.1;
    }
}

//---------------------------------------------------------------------------//
// Unit tests.
//---------------------------------------------------------------------------//
// All points will be in the mesh in this test.
TEUCHOS_UNIT_TEST( SharedDomainMap, shared_domain_map_test8 )
{
    using namespace DataTransferKit;

    // Setup communication.
    Teuchos::RCP< const Teuchos::Comm<int> > comm = getDefaultComm<int>();
    int my_rank = comm->getRank();
    int my_size = comm->getSize();

    // Setup source mesh manager.
    int edge_size = 4;
    Teuchos::ArrayRCP<Teuchos::RCP<MeshBlock> > mesh_blocks( 1 );
    mesh_blocks[0] = buildMyMesh( my_rank, my_size, edge_size );
    Teuchos::RCP< MeshManager > source_mesh_manager = Teuchos::rcp( 
	new MeshManager( mesh_blocks, comm, 3 ) );

    // Setup target coordinate field manager.
    int num_points = 1000;
    int point_dim = 3;
    Teuchos::RCP<MyField> coordinate_field = 
	Teuchos::rcp( new MyField( num_points, point_dim ) );
    buildCoordinateField( my_rank, my_size, num_points, edge_size,
			  coordinate_field );
    Teuchos::RCP< FieldManager<MyField> > target_coord_manager = 
	Teuchos::rcp( new FieldManager<MyField>( coordinate_field, comm ) );

    // Create field evaluator.
    Teuchos::RCP< FieldEvaluator<MeshId,MyField> > 
	source_evaluator = 
    	Teuchos::rcp( new MyEvaluator( mesh_blocks[0], comm ) );

    // Create data target. This target is a scalar.
    int target_dim = 1;
    Teuchos::RCP<MyField> target_field =  
	Teuchos::rcp( new MyField( num_points, target_dim ) );
    Teuchos::RCP< FieldManager<MyField> > target_space_manager = Teuchos::rcp( 
	new FieldManager<MyField>( target_field, comm ) );

    // Setup and apply the shared domain mapping.
    SharedDomainMap<MyField> shared_domain_map( 
	comm, source_mesh_manager->dim() );
    shared_domain_map.setup( source_mesh_manager, target_coord_manager );
    shared_domain_map.apply( source_evaluator, target_space_manager );

    // Check the data transfer. Each target point should have been assigned
    // its source rank + 1 as data.
    int source_rank;
    for ( int n = 0; n < num_points; ++n )
    {
	source_rank = std::floor(*(coordinate_field->begin()+n) / (edge_size-1));
	TEST_ASSERT( source_rank+1 == 
		     *(target_space_manager->field()->begin()+n) );
    }
}

//---------------------------------------------------------------------------//
// Some points will be outside of the mesh in this test.
TEUCHOS_UNIT_TEST( SharedDomainMap, shared_domain_map_expanded_test8 )
{
    using namespace DataTransferKit;

    // Setup communication.
    Teuchos::RCP< const Teuchos::Comm<int> > comm = getDefaultComm<int>();
    int my_rank = comm->getRank();
    int my_size = comm->getSize();

    // Setup source mesh manager.
    int edge_size = 4;
    Teuchos::ArrayRCP<Teuchos::RCP<MeshBlock> > mesh_blocks( 1 );
    mesh_blocks[0] = buildMyMesh( my_rank, my_size, edge_size );
    Teuchos::RCP< MeshManager > source_mesh_manager = Teuchos::rcp( 
	new MeshManager( mesh_blocks, comm, 3 ) );

    // Setup target coordinate field manager.
    int num_points = 1000;
    int point_dim = 3;
    Teuchos::RCP<MyField> coordinate_field =
	Teuchos::rcp( new MyField( num_points, point_dim ) );
    buildExpandedCoordinateField( my_rank, my_size, num_points, edge_size,
				  coordinate_field );
    Teuchos::RCP< FieldManager<MyField> > target_coord_manager = 
	Teuchos::rcp( new FieldManager<MyField>( coordinate_field, comm ) );

    // Create field evaluator.
    Teuchos::RCP< FieldEvaluator<MeshId,MyField> >
	source_evaluator = 
    	Teuchos::rcp( new MyEvaluator( mesh_blocks[0], comm ) );

    // Create data target. This target is a scalar.
    int target_dim = 1;
    Teuchos::RCP<MyField> target_field =  
	Teuchos::rcp( new MyField( num_points, target_dim ) );
    Teuchos::RCP< FieldManager<MyField> > target_space_manager = Teuchos::rcp( 
	new FieldManager<MyField>( target_field, comm ) );

    // Setup and apply the shared domain mapping.
    SharedDomainMap<MyField> shared_domain_map( 
	comm, source_mesh_manager->dim(), true );
    shared_domain_map.setup( source_mesh_manager, target_coord_manager );
    shared_domain_map.apply( source_evaluator, target_space_manager );

    // Check the data transfer. Each target point should have been assigned
    // its source rank + 1 as data if it is in the mesh and 0.0 if it is outside.
    int source_rank;
    Teuchos::Array<MeshId> missing_points;
    for ( int n = 0; n < num_points; ++n )
    {
	if ( *(coordinate_field->begin()+n) < 0.0 ||
	     *(coordinate_field->begin()+n) > (edge_size-1)*my_size ||
	     *(coordinate_field->begin()+n+num_points) < 0.0 ||
	     *(coordinate_field->begin()+n+num_points) > edge_size-1 ||
	     *(coordinate_field->begin()+n+2*num_points) < 0.0 ||
	     *(coordinate_field->begin()+n+2*num_points) > 1.0 )
	{
	    missing_points.push_back(n);	
	    TEST_ASSERT( 0.0 == *(target_space_manager->field()->begin()+n) );
	}
	else
	{
	    source_rank = std::floor(target_coord_manager->field()->getData()[n] 
	    			     / (edge_size-1));
	    TEST_ASSERT( source_rank+1 == 
	    		 target_space_manager->field()->getData()[n] );
	}
    }

    // Check the missing points.
    TEST_ASSERT( missing_points.size() > 0 );
    Teuchos::ArrayView<MeshId> missed_in_map = 
	shared_domain_map.getMissedTargetPoints();
    TEST_ASSERT( missing_points.size() == missed_in_map.size() );

    std::sort( missing_points.begin(), missing_points.end() );
    std::sort( missed_in_map.begin(), missed_in_map.end() );

    for ( int n = 0; n < (int) missing_points.size(); ++n )
    {
	TEST_ASSERT( missing_points[n] == missed_in_map[n] );
    }
}

//---------------------------------------------------------------------------//
// Some points will be outside of the mesh in this test. The mesh is not
// rectilinear so we will be sure to search the kD-tree with points that
// aren't in the mesh.
TEUCHOS_UNIT_TEST( SharedDomainMap, shared_domain_map_tiled_test8 )
{
    using namespace DataTransferKit;

    // Setup communication.
    Teuchos::RCP< const Teuchos::Comm<int> > comm = getDefaultComm<int>();
    int my_rank = comm->getRank();
    int my_size = comm->getSize();

    // Setup source mesh manager.
    int edge_size = 4;
    Teuchos::ArrayRCP<Teuchos::RCP<MeshBlock> > mesh_blocks( 1 );
    mesh_blocks[0] = buildTiledMesh( my_rank, my_size, edge_size );
    Teuchos::RCP< MeshManager > source_mesh_manager = Teuchos::rcp( 
	new MeshManager( mesh_blocks, comm, 3 ) );

    // Setup target coordinate field manager.
    int num_points = 1000;
    int point_dim = 3;
    Teuchos::RCP<MyField> coordinate_field = 
	Teuchos::rcp( new MyField( num_points, point_dim ) );
    buildTiledCoordinateField( my_rank, my_size, num_points, edge_size,
			       coordinate_field );
    Teuchos::RCP< FieldManager<MyField> > target_coord_manager = 
	Teuchos::rcp( new FieldManager<MyField>( coordinate_field, comm ) );

    // Create field evaluator.
    Teuchos::RCP< FieldEvaluator<MeshId,MyField> > 
	source_evaluator = 
    	Teuchos::rcp( new MyEvaluator( mesh_blocks[0], comm ) );

    // Create data target. This target is a scalar.
    int target_dim = 1;
    Teuchos::RCP<MyField> target_field =  
	Teuchos::rcp( new MyField( num_points, target_dim ) );
    Teuchos::RCP< FieldManager<MyField> > target_space_manager = Teuchos::rcp( 
	new FieldManager<MyField>( target_field, comm ) );

    // Setup and apply the shared domain mapping.
    SharedDomainMap<MyField> shared_domain_map( 
	comm, source_mesh_manager->dim(), true );
    shared_domain_map.setup( source_mesh_manager, target_coord_manager );
    shared_domain_map.apply( source_evaluator, target_space_manager );

    // Check the data transfer. Each target point should have been assigned
    // its source rank + 1 as data if it is in the mesh and 0.0 if it is
    // outside.
    int source_rank;
    Teuchos::Array<MeshId> missing_points;
    bool tagged;
    for ( int n = 0; n < num_points; ++n )
    {
	tagged = false;

	if ( *(coordinate_field->begin()+n) < 0.0 ||
	     *(coordinate_field->begin()+n) > (edge_size-1)*my_size ||
	     *(coordinate_field->begin()+n+num_points) < 0.0 ||
	     *(coordinate_field->begin()+n+num_points) > (edge_size-1)*my_size ||
	     *(coordinate_field->begin()+n+2*num_points) < 0.0 ||
	     *(coordinate_field->begin()+n+2*num_points) > 1.0 )
	{
	    missing_points.push_back(n);	
	    TEST_ASSERT( 0.0 == *(target_space_manager->field()->begin()+n) );
	    tagged = true;
	}
	
	else
	{
	    for ( int i = 0; i < my_size; ++i )
	    {
		if ( *(coordinate_field->begin()+n) >= (edge_size-1)*i &&
		     *(coordinate_field->begin()+n) <= (edge_size-1)*(i+1) &&
		     *(coordinate_field->begin()+n+num_points) >=
		     (edge_size-1)*i &&
		     *(coordinate_field->begin()+n+num_points) <= 
		     (edge_size-1)*(i+1) &&
		     *(coordinate_field->begin()+n+2*num_points) >= 0.0 &&
		     *(coordinate_field->begin()+n+2*num_points) <= 1.0 && !tagged )
		{
		    source_rank = 
			std::floor(target_coord_manager->field()->getData()[n] 
					     / (edge_size-1));
		    TEST_ASSERT( source_rank+1 == 
				 target_space_manager->field()->getData()[n] );
		    tagged = true;
		}
	    }

	    if ( !tagged) 
	    {
		missing_points.push_back(n);	
		TEST_ASSERT( 0.0 == *(target_space_manager->field()->begin()+n) );
		tagged = true;
	    }
	}

	TEST_ASSERT( tagged );
    }

    // Check the missing points.
    TEST_ASSERT( missing_points.size() > 0 );
    Teuchos::ArrayView<MeshId> missed_in_map = 
	shared_domain_map.getMissedTargetPoints();
    TEST_ASSERT( missing_points.size() == missed_in_map.size() );

    std::sort( missing_points.begin(), missing_points.end() );
    std::sort( missed_in_map.begin(), missed_in_map.end() );

    for ( int n = 0; n < (int) missing_points.size(); ++n )
    {
	TEST_ASSERT( missing_points[n] == missed_in_map[n] );
    }
}

//---------------------------------------------------------------------------//
// end tstSharedDomainMap8.cpp
//---------------------------------------------------------------------------//
