//---------------------------------------------------------------------------//
/*!
 * \file DTK_RendezvousMesh_def.hpp
 * \author Stuart R. Slattery
 * \brief Concrete Moab mesh template definitions.
 */
//---------------------------------------------------------------------------//

#ifndef DTK_RENDEZVOUSMESH_DEF_HPP
#define DTK_RENDEZVOUSMESH_DEF_HPP

#include <vector>
#include <algorithm>
#include <cassert>

#include <DTK_Exception.hpp>
#include <DTK_MeshTraits.hpp>
#include <DTK_FieldTraits.hpp>

#include <MBCore.hpp>

#include <Teuchos_ENull.hpp>

namespace DataTransferKit
{
//---------------------------------------------------------------------------//
/*!
 * \brief Constructor.
 */
template<typename Handle>
RendezvousMesh<Handle>::RendezvousMesh( const RCP_Moab& moab, 
					const moab::Range& elements,
					const HandleMap& handle_map )
    : d_moab( moab )
    , d_elements( elements )
    , d_handle_map( handle_map )
{ /* ... */ }

//---------------------------------------------------------------------------//
/*!
 * \brief Destructor.
 */
template<typename Handle>
RendezvousMesh<Handle>::~RendezvousMesh()
{ /* ... */ }

//---------------------------------------------------------------------------//
// Non-member creation methods.
//---------------------------------------------------------------------------//
/*!
 * \brief Create a RendezvousMesh from an object that implements mesh traits.
 */
template<typename Mesh>
Teuchos::RCP< RendezvousMesh<typename MeshTraits<Mesh>::handle_type> > 
createRendezvousMesh( const Mesh& mesh )
{
    // Setup types and iterators
    typedef typename MeshTraits<Mesh>::handle_type handle_type;
    typename MeshTraits<Mesh>::const_handle_iterator handle_iterator;
    typename MeshTraits<Mesh>::const_handle_iterator conn_iterator;
    typename MeshTraits<Mesh>::const_coordinate_iterator coord_iterator;

    // Create a moab interface.
    moab::ErrorCode error;
    Teuchos::RCP<moab::Interface> moab = Teuchos::rcp( new moab::Core() );
    testPostcondition( moab != Teuchos::null,
		       "Error creating MOAB interface" );

    // Check the nodes and coordinates for consistency.
    int num_nodes = std::distance( MeshTraits<Mesh>::nodesBegin( mesh ),
				   MeshTraits<Mesh>::nodesEnd( mesh ) );
    int num_coords = std::distance( MeshTraits<Mesh>::coordsBegin( mesh ),
				    MeshTraits<Mesh>::coordsEnd( mesh ) );
    testInvariant( num_coords == 3 * num_nodes,
		   "Number of coordinates provided != 3 * number of nodes" );

    // Add the source mesh nodes to moab. The coordinates must be interleaved.
    moab::Range vertices;
    if ( MeshTraits<Mesh>::interleavedCoordinates( mesh ) )
    {
	error = moab->create_vertices(
	    &( *MeshTraits<Mesh>::coordsBegin( mesh ) ),
	    num_nodes, vertices );
	testInvariant( moab::MB_SUCCESS == error, 
		       "Failed to create vertices in MOAB." );
    }
    else
    {
	std::vector<double> interleaved_coords( num_coords );
	typename MeshTraits<Mesh>::const_coordinate_iterator coord_iterator;
	int i = 0;	
	int node, dim;
	for ( coord_iterator = MeshTraits<Mesh>::coordsBegin( *mesh );
	      coord_iterator != MeshTraits<Mesh>::coordsEnd( *mesh );
	      ++coord_iterator, ++i )
	{
	    dim = std::floor( i / num_nodes );
	    node = i - dim*num_nodes;
	    interleaved_coords[ 3*node + dim ] = *coord_iterator
	}
	error = moab->create_vertices( 
	    &interleaved_coords[0], num_nodes, vertices );
	testInvariant( moab::MB_SUCCESS == error, 
		       "Failed to create vertices in MOAB." );
    }
    testPostcondition( !vertices.empty(),
		       "Vertex range is empty." );
    assert( (int) vertices.size() == num_nodes );

    // Map the native vertex handles to the moab vertex handles.
    moab::Range::const_iterator range_iterator;
    std::map<handle_type,moab::EntityHandle> vertex_handle_map;
    for ( range_iterator = vertices.begin(),
	 handle_iterator = MeshTraits<Mesh>::nodesBegin( mesh );
	  range_iterator != vertices.end();
	  ++range_iterator, ++handle_iterator )
    {
	vertex_handle_map[ *handle_iterator ] = *range_iterator;
    }

    // Check the elements and connectivity for consistency.
    int nodes_per_element = 
	MeshTraits<Mesh>::nodesPerElement( mesh );
    int num_elements = std::distance( MeshTraits<Mesh>::elementsBegin( mesh ),
				      MeshTraits<Mesh>::elementsEnd( mesh ) );
    int num_connect = std::distance( MeshTraits<Mesh>::connectivityBegin( mesh ),
				     MeshTraits<Mesh>::connectivityEnd( mesh ) );
    testInvariant( num_elements == num_connect / nodes_per_element &&
		   num_connect % nodes_per_element == 0,
		   "Connectivity array inconsistent with element description." );

    // Extract the source mesh elements and add them to moab.
    moab::Range moab_elements;
    std::vector<moab::EntityHandle> element_connectivity;
    std::map<moab::EntityHandle,handle_type> element_handle_map;
    for ( handle_iterator = MeshTraits<Mesh>::elementsBegin( mesh ),
	    conn_iterator = MeshTraits<Mesh>::connectivityBegin( mesh );
	  handle_iterator != MeshTraits<Mesh>::elementsEnd( mesh );
	  ++handle_iterator )
    {
	// Extract the connecting nodes for this element.
	element_connectivity.clear();
	for ( int n = 0; n < nodes_per_element; ++n, ++conn_iterator )
	{
	    element_connectivity.push_back( vertex_handle_map[*conn_iterator] );
	}
	testInvariant( (int) element_connectivity.size() == nodes_per_element,
		       "Element connectivity size != nodes per element." );

	// Creat the element in moab.
	moab::EntityType entity_type = moab_topology_table[ 
	    MeshTraits<Mesh>::elementTopology( mesh ) ];
	moab::EntityHandle moab_element;
	error = moab->create_element( entity_type,
				      &element_connectivity[0],
				      element_connectivity.size(),
				      moab_element );
	testInvariant( moab::MB_SUCCESS == error,
		       "Failed to create element in MOAB." );
	moab_elements.insert( moab_element );

	// Map the moab element handle to the native element handle.
	element_handle_map[ moab_element ] = *handle_iterator;
    }
    
    // Create and return the mesh.
    return Teuchos::rcp( new RendezvousMesh<handle_type>( 
			     moab, moab_elements, element_handle_map ) );
}

//---------------------------------------------------------------------------//

} // end namespace DataTransferKit

#endif // end DTK_RENDEZVOUSMESH_DEF_HPP

//---------------------------------------------------------------------------//
// end DTK_RendezvousMesh_def.hpp
//---------------------------------------------------------------------------//
