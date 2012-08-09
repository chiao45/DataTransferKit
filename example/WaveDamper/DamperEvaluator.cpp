//---------------------------------------------------------------------------//
/*!
 * \file DamperEvaluator.cpp
 * \author Stuart R. Slattery
 * \brief Damper code evaluators for DTK.
 */
//---------------------------------------------------------------------------//

#include "DamperEvaluator.hpp"

//---------------------------------------------------------------------------//
/*!
 * \brief Damper function evaluator.
 */
DamperEvaluator::base_type::field_type
DamperEvaluator::evaluate( 
    const Teuchos::ArrayRCP<base_type::GlobalOrdinal>& elements,
    const Teuchos::ArrayRCP<double>& coords )
{
    // Setup a field to apply the data to.
    int field_size = elements.size();
    Teuchos::ArrayRCP<double> eval_data( field_size );
    base_type::field_type evaluations( eval_data, 1 );

    // Get the grid.
    std::vector<double> grid = d_damper->get_grid();
    int num_grid_elements = grid.size() - 1;

    // Get the damper data.
    std::vector<double> data = d_damper->get_damping();

    // Interpolate the local solution onto the given coordinates.
    Teuchos::ArrayRCP<int>::iterator element_iterator;
    int eval_id;
    for ( element_iterator = elements.begin(); 
	  element_iterator != elements.end();
	  ++element_iterator )
    {
	eval_id = std::distance( elements.begin(), element_iterator );

	// If this is a valid element id for this process, interpolate with a
	// linear basis.
	if ( *element_iterator < num_grid_elements )
	{
	    eval_data[eval_id] = data[eval_id]
				 + ( coords[eval_id] - grid[eval_id] )
				 * ( data[eval_id+1] - data[eval_id] )
				 / ( grid[eval_id+1] - grid[eval_id] );
	}

	// Otherwise put a zero for this element/coordinate pair per the
	// FieldEvaluator documentation.
	else
	{
	    eval_data[eval_id] = 0.0;
	}
    }

    return evaluations;
}

//---------------------------------------------------------------------------//
// end DamperEvaluator.cpp
//---------------------------------------------------------------------------//

