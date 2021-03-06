//---------------------------------------------------------------------------//
/*
  Copyright (c) 2012, Stuart R. Slattery
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  *: Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  *: Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  *: Neither the name of the University of Wisconsin - Madison nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
//---------------------------------------------------------------------------//
/*!
 * \file   tstNodeToNodeOperator.cpp
 * \author Stuart R. Slattery
 * \brief  NodeToNode operator tests.
 */
//---------------------------------------------------------------------------//

#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <DTK_BasicGeometryManager.hpp>
#include <DTK_DBC.hpp>
#include <DTK_Entity.hpp>
#include <DTK_EntityCenteredField.hpp>
#include <DTK_FieldMultiVector.hpp>
#include <DTK_MapOperatorFactory.hpp>
#include <DTK_Point.hpp>

#include "Teuchos_Array.hpp"
#include "Teuchos_ArrayRCP.hpp"
#include "Teuchos_CommHelpers.hpp"
#include "Teuchos_DefaultComm.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Teuchos_Ptr.hpp"
#include "Teuchos_RCP.hpp"
#include "Teuchos_UnitTestHarness.hpp"
#include "Teuchos_XMLParameterListCoreHelpers.hpp"

//---------------------------------------------------------------------------//
// Test epsilon.
//---------------------------------------------------------------------------//

const double epsilon = 1.0e-14;

//---------------------------------------------------------------------------//
// Test dirver.
//---------------------------------------------------------------------------//
void setupAndRunTest( const std::string &input_file,
                      Teuchos::Array<double> &gold_data,
                      Teuchos::Array<double> &test_result,
                      const bool perturbation )
{
    // Get the test parameters.
    Teuchos::RCP<Teuchos::ParameterList> parameters =
        Teuchos::rcp( new Teuchos::ParameterList() );
    Teuchos::updateParametersFromXmlFile( input_file,
                                          Teuchos::inoutArg( *parameters ) );

    // Get the communicator.
    Teuchos::RCP<const Teuchos::Comm<int>> comm =
        Teuchos::DefaultComm<int>::getComm();
    int comm_rank = comm->getRank();
    int comm_size = comm->getSize();
    int inverse_rank = comm_size - comm_rank - 1;
    const int space_dim = 3;
    int field_dim = 1;

    // Make a set of domain points. These span 0-1 in y and z and span
    // comm_rank-comm_rank+1 in x. The value of the field we are transferring
    // is the x + y + z coordinate of the points.
    int num_points = 10;
    int domain_mult = 100;
    Teuchos::Array<DataTransferKit::Entity> domain_points( num_points );
    Teuchos::Array<double> coords( space_dim );
    DataTransferKit::EntityId point_id = 0;
    Teuchos::ArrayRCP<double> domain_data( field_dim * num_points );
    double coord_val = 0.0;
    for ( int i = 0; i < num_points; ++i )
    {
        point_id = num_points * comm_rank + i;
        coord_val = static_cast<double>( i ) / num_points;
        coords[0] = coord_val + comm_rank;
        coords[1] = coord_val;
        coords[2] = coord_val;
        domain_points[i] =
            DataTransferKit::Point( point_id, comm_rank, coords );
        domain_data[i] = coords[0] + coords[1] + coords[2];
    }

    // Make a set of range points. These span 0-1 in y and z and span
    // comm_rank-inverse_rank+1 in x. These are the same points as the domain
    // but with a different parallel decomposition. The gold data is the
    // expected result of the interpolation.
    Teuchos::Array<DataTransferKit::Entity> range_points( num_points );
    test_result.resize( field_dim * num_points );
    gold_data.resize( num_points );
    double offset = perturbation ? 1.0e-6 : 0.0;
    for ( int i = 0; i < num_points; ++i )
    {
        point_id = num_points * inverse_rank + i + 1;
        coord_val = static_cast<double>( i ) / num_points + offset;
        coords[0] = coord_val + inverse_rank;
        coords[1] = coord_val;
        coords[2] = coord_val;
        range_points[i] = DataTransferKit::Point( point_id, comm_rank, coords );
        test_result[i] = 0.0;
        gold_data[i] = coords[0] + coords[1] + coords[2] - 3 * offset;
    }

    // Make a manager for the domain geometry.
    DataTransferKit::BasicGeometryManager domain_manager( comm, space_dim,
                                                          domain_points() );

    // Make a manager for the range geometry.
    DataTransferKit::BasicGeometryManager range_manager( comm, space_dim,
                                                         range_points() );

    // Make a DOF vector for the domain.
    Teuchos::RCP<DataTransferKit::Field> domain_field =
        Teuchos::rcp( new DataTransferKit::EntityCenteredField(
            domain_points(), field_dim, domain_data,
            DataTransferKit::EntityCenteredField::BLOCKED ) );
    Teuchos::RCP<Tpetra::MultiVector<double, int, DataTransferKit::SupportId>>
        domain_vector = Teuchos::rcp( new DataTransferKit::FieldMultiVector(
            domain_field, domain_manager.functionSpace()->entitySet() ) );

    // Make a DOF vector for the range.
    Teuchos::RCP<DataTransferKit::Field> range_field =
        Teuchos::rcp( new DataTransferKit::EntityCenteredField(
            range_points(), field_dim, Teuchos::arcpFromArray( test_result ),
            DataTransferKit::EntityCenteredField::BLOCKED ) );
    Teuchos::RCP<Tpetra::MultiVector<double, int, DataTransferKit::SupportId>>
        range_vector = Teuchos::rcp( new DataTransferKit::FieldMultiVector(
            range_field, range_manager.functionSpace()->entitySet() ) );

    // Create the point cloud operator
    DataTransferKit::MapOperatorFactory factory;
    Teuchos::RCP<DataTransferKit::MapOperator> cloud_op = factory.create(
        domain_vector->getMap(), range_vector->getMap(), *parameters );

    // Setup the operator.
    cloud_op->setup( domain_manager.functionSpace(),
                     range_manager.functionSpace() );

    // Apply the operator.
    cloud_op->apply( *domain_vector, *range_vector );
}

//---------------------------------------------------------------------------//
// Tests.
//---------------------------------------------------------------------------//
TEUCHOS_UNIT_TEST( NodeToNodeOperator, matching_node_test )
{
    // Run the test.
    Teuchos::Array<double> gold_data;
    Teuchos::Array<double> test_result;
    setupAndRunTest( "matching_node_test.xml", gold_data, test_result, false );

    // Check the results.
    TEST_EQUALITY( gold_data.size(), test_result.size() );
    int num_points = gold_data.size();
    for ( int i = 0; i < num_points; ++i )
    {
        TEST_FLOATING_EQUALITY( gold_data[i], test_result[i], epsilon );
    }
}

//---------------------------------------------------------------------------//
TEUCHOS_UNIT_TEST( NodeToNodeOperator, non_matching_node_test )
{
    // Run the test.
    Teuchos::Array<double> gold_data;
    Teuchos::Array<double> test_result;
    setupAndRunTest( "non_matching_node_test.xml", gold_data, test_result,
                     true );

    // Check the results.
    TEST_EQUALITY( gold_data.size(), test_result.size() );
    int num_points = gold_data.size();
    for ( int i = 0; i < num_points; ++i )
    {
        TEST_FLOATING_EQUALITY( gold_data[i], test_result[i], epsilon );
    }
}

//---------------------------------------------------------------------------//
TEUCHOS_UNIT_TEST( NodeToNodeOperator, exception_test )
{
#if HAVE_DTK_DBC
    // Run the test assuming matching nodes but add a perturbation.
    Teuchos::Array<double> gold_data;
    Teuchos::Array<double> test_result;

    TEST_THROW( setupAndRunTest( "matching_node_test.xml", gold_data,
                                 test_result, true ),
                DataTransferKit::DataTransferKitException );
#endif
}

//---------------------------------------------------------------------------//
// end tstSplineInterpolation.cpp
//---------------------------------------------------------------------------//
