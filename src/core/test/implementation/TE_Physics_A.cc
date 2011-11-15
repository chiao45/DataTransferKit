//----------------------------------*-C++-*----------------------------------//
/*!
 * \file   implementation/TE_Physics_A.cc
 * \author Stuart Slattery 
 * \date   Wed Oct 05 10:57:30 2011
 * \brief  Transfer Evaluator for Physics_A.
 */
//---------------------------------------------------------------------------//
// $Id: template.cc,v 1.3 2008/01/02 17:18:47 9te Exp $
//---------------------------------------------------------------------------//

#include "TE_Physics_A.hh"

namespace coupler
{
//---------------------------------------------------------------------------//
// Constructor.
TE_Physics_A::TE_Physics_A(physics_A::Physics_A* a_)
    : a(a_)
{ /* ... */ }

//---------------------------------------------------------------------------//
// Destructor.
TE_Physics_A::~TE_Physics_A()
{ /* ... */ }

//---------------------------------------------------------------------------//
//! Register a communication object.
void TE_Physics_A::register_comm(Communicator &comm)
{
    comm = a->comm();
}

//---------------------------------------------------------------------------//
//! Register a field associated with the entities.
bool TE_Physics_A::register_field(std::string field_name)
{
    if (field_name == "ORDINATE") return true;
    else return false;
}

//---------------------------------------------------------------------------//
// Given (x,y,z) coordinates and an associated globally unique handle,
// return true if in the local domain, false if not.
bool TE_Physics_A::find_xyz(double x, 
			    double y, 
			    double z,
			    Handle handle)
{
    return a->get_xy_info(x, y, handle);
}

//---------------------------------------------------------------------------//
// Given an entity handle, get the field data associated with that handle.
void TE_Physics_A::pull_data(std::string field_name,
			     Handle handle,
			     double &data)
{

    if (field_name == "ORDINATE")
    {
	a->get_state(handle, data);
    }
}

//---------------------------------------------------------------------------//

} // end namespace coupler

//---------------------------------------------------------------------------//
//                 end of TE_Physics_A.cc
//---------------------------------------------------------------------------//