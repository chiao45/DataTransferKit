/****************************************************************************
 * Copyright (c) 2012-2017 by the DataTransferKit authors                   *
 * All rights reserved.                                                     *
 *                                                                          *
 * This file is part of the DataTransferKit library. DataTransferKit is     *
 * distributed under a BSD 3-clause license. For the licensing terms see    *
 * the LICENSE file in the top-level directory.                             *
 ****************************************************************************/

#include <DTK_DetailsUtils.hpp>

#include <Teuchos_UnitTestHarness.hpp>

#include <algorithm>
#include <vector>

TEUCHOS_UNIT_TEST_TEMPLATE_1_DECL( DetailsUtils, fill, DeviceType )
{
    int const n = 10;
    Kokkos::View<float *, DeviceType> v( "v", n );
    float const pi = 3.14;
    DataTransferKit::fill( v, pi );
    auto v_host = Kokkos::create_mirror_view( v );
    Kokkos::deep_copy( v_host, v );
    TEST_COMPARE_ARRAYS( std::vector<float>( n, pi ), v_host );
}

TEUCHOS_UNIT_TEST_TEMPLATE_1_DECL( DetailsUtils, prefix_sum, DeviceType )
{
    int const n = 10;
    Kokkos::View<int *, DeviceType> x( "x", n );
    std::vector<int> x_ref( n, 1 );
    x_ref.back() = 0;
    auto x_host = Kokkos::create_mirror_view( x );
    for ( int i = 0; i < n; ++i )
        x_host( i ) = x_ref[i];
    Kokkos::deep_copy( x, x_host );
    Kokkos::View<int *, DeviceType> y( "y", n );
    DataTransferKit::exclusive_prefix_sum( x, y );
    std::vector<int> y_ref( n );
    std::iota( y_ref.begin(), y_ref.end(), 0 );
    auto y_host = Kokkos::create_mirror_view( y );
    Kokkos::deep_copy( y_host, y );
    Kokkos::deep_copy( x_host, x );
    TEST_COMPARE_ARRAYS( y_host, y_ref );
    TEST_COMPARE_ARRAYS( x_host, x_ref );
    // in-place
    DataTransferKit::exclusive_prefix_sum( x, x );
    Kokkos::deep_copy( x_host, x );
    TEST_COMPARE_ARRAYS( x_host, y_ref );
    int const m = 11;
    TEST_INEQUALITY( n, m );
    Kokkos::View<int *, DeviceType> z( "z", m );
    TEST_THROW( DataTransferKit::exclusive_prefix_sum( x, z ),
                DataTransferKit::DataTransferKitException );
}

TEUCHOS_UNIT_TEST_TEMPLATE_1_DECL( DetailsUtils, last_element, DeviceType )
{
    Kokkos::View<int *, DeviceType> v( "v", 2 );
    auto v_host = Kokkos::create_mirror_view( v );
    v_host( 0 ) = 33;
    v_host( 1 ) = 24;
    Kokkos::deep_copy( v, v_host );
    TEST_EQUALITY( DataTransferKit::last_element( v ), 24 );
    Kokkos::View<int *, DeviceType> w( "w", 0 );
    TEST_THROW( DataTransferKit::last_element( w ),
                DataTransferKit::DataTransferKitException );
}

// Include the test macros.
#include "DataTransferKitSearch_ETIHelperMacros.h"

// Create the test group
#define UNIT_TEST_GROUP( NODE )                                                \
    using DeviceType##NODE = typename NODE::device_type;                       \
    TEUCHOS_UNIT_TEST_TEMPLATE_1_INSTANT( DetailsUtils, fill,                  \
                                          DeviceType##NODE )                   \
    TEUCHOS_UNIT_TEST_TEMPLATE_1_INSTANT( DetailsUtils, prefix_sum,            \
                                          DeviceType##NODE )                   \
    TEUCHOS_UNIT_TEST_TEMPLATE_1_INSTANT( DetailsUtils, last_element,          \
                                          DeviceType##NODE )

// Demangle the types
DTK_ETI_MANGLING_TYPEDEFS()

// Instantiate the tests
DTK_INSTANTIATE_N( UNIT_TEST_GROUP )
