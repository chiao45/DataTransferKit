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
 * \file   DTK_SplineInterpolationPairing_impl.hpp
 * \author Stuart R. Slattery
 * \brief  Local child/parent center pairings.
 */
//---------------------------------------------------------------------------//

#ifndef DTK_SPLINEINTERPOLATIONPAIRING_IMPL_HPP
#define DTK_SPLINEINTERPOLATIONPAIRING_IMPL_HPP

#include <algorithm>

#include "DTK_DBC.hpp"
#include "DTK_EuclideanDistance.hpp"
#include "DTK_SplineInterpolationPairing.hpp"
#include "DTK_StaticSearchTree.hpp"

namespace DataTransferKit
{
//---------------------------------------------------------------------------//
/*!
 * \brief Constructor.
 */
// added the leaf parameter, QC
template <int DIM>
SplineInterpolationPairing<DIM>::SplineInterpolationPairing(
    const Teuchos::ArrayView<const double> &child_centers,
    const Teuchos::ArrayView<const double> &parent_centers, const bool use_knn,
    const unsigned num_neighbors, const double radius, const int leaf,
    const bool use_new_search )
{
    DTK_REQUIRE( 0 == child_centers.size() % DIM );
    DTK_REQUIRE( 0 == parent_centers.size() % DIM );

    // Setup the kD-tree
    unsigned leaf_size = 30;
    if ( leaf > 0 )
    {
        leaf_size = (unsigned)leaf;
    }
    NanoflannTree<DIM> tree( child_centers, leaf_size );

    // Allocate arrays
    unsigned num_parents = parent_centers.size() / DIM;
    d_pairings.resize( num_parents );
    d_pair_sizes = Teuchos::ArrayRCP<EntityId>( num_parents );
    d_radii.resize( num_parents );
    d_hs.resize( num_parents );

    // added QC
    // TODO this part should be moved in the kd-tree
    // we first search for a small neighborhood by KNN, the following defines
    // the table that estimate the one-ring neighbor
    const static int SMALL_KNN[3] = {3, 6, 7};
    const static int small_knn = SMALL_KNN[DIM - 1];
    // then we use the maximal length to define radius that expand by 5 times
    // to query the mesh
    // NOTE that this still complete resolves issues with stretched grids...

    if ( use_new_search )
    {
        for ( unsigned i = 0; i < num_parents; ++i )
        {
            // Not efficient in terms of memory allocation, but we have
            // no choice here
            const Teuchos::Array<unsigned> small_nbr =
                tree.nnSearch( parent_centers( DIM * i, DIM ), small_knn );
            double h = 0.0;
            for ( auto itr = small_nbr.begin(); itr != small_nbr.end(); ++itr )
                h = std::max(
                    h, EuclideanDistance<DIM>::distance(
                           parent_centers( DIM * i, DIM ).getRawPtr(),
                           child_centers( DIM * *itr, DIM ).getRawPtr() ) );
            d_radii[i] = 5.1 * h; // expand 5times+10%, might be too large!
            d_pairings[i] =
                tree.radiusSearch( parent_centers( DIM * i, DIM ), d_radii[i] );

            // Get the size of the support.
            d_pair_sizes[i] = d_pairings[i].size();

            // added,QC
            // computing the closest h
            d_hs[i] = EuclideanDistance<DIM>::distance(
                parent_centers( DIM * i, DIM ).getRawPtr(),
                child_centers( DIM * d_pairings[i].front(), DIM ).getRawPtr() );
        }
    }
    // added QC
    else
    {
        // Search for pairs
        for ( unsigned i = 0; i < num_parents; ++i )
        {

            // If kNN do the nearest neighbor search for kNN and calculate a
            // radius. The radius will be a small fraction larger than the
            // farthest neighbor. An alternative to this would be to find the
            // kNN+1 neighbors and use the last neighbor's distance as the
            // radius.
            if ( use_knn )
            {
                // Get the knn neighbors
                d_pairings[i] = tree.nnSearch( parent_centers( DIM * i, DIM ),
                                               num_neighbors );

                // Get the radius from kNN. Make it slightly larger so the last
                // neighbor gives a non-zero contribution to the interpolant.
                d_radii[i] = EuclideanDistance<DIM>::distance(
                    parent_centers( DIM * i, DIM ).getRawPtr(),
                    child_centers( DIM * d_pairings[i].back(), DIM )
                        .getRawPtr() );
                d_radii[i] *= 1.01;
            }

            // Otherwise do the radius search.
            else
            {
                d_pairings[i] =
                    tree.radiusSearch( parent_centers( DIM * i, DIM ), radius );
                d_radii[i] = radius;
            }

            // Get the size of the support.
            d_pair_sizes[i] = d_pairings[i].size();

            // added,QC
            // computing the closest h
            d_hs[i] = EuclideanDistance<DIM>::distance(
                parent_centers( DIM * i, DIM ).getRawPtr(),
                child_centers( DIM * d_pairings[i].front(), DIM ).getRawPtr() );
        }
    }
}

//---------------------------------------------------------------------------//
/*!
 * \brief Given a parent center local id get the ids of the child centers
 * within the given radius.
 */
template <int DIM>
Teuchos::ArrayView<const unsigned>
SplineInterpolationPairing<DIM>::childCenterIds(
    const unsigned parent_id ) const
{
    DTK_REQUIRE( parent_id < d_pairings.size() );
    Teuchos::ArrayView<const unsigned> id_view = d_pairings[parent_id]();
    return id_view;
}

//---------------------------------------------------------------------------//
/*
 * \brief Get the support radius of a given parent.
 */
template <int DIM>
double SplineInterpolationPairing<DIM>::parentSupportRadius(
    const unsigned parent_id ) const
{
    DTK_REQUIRE( parent_id < d_radii.size() );
    return d_radii[parent_id];
}

//---------------------------------------------------------------------------//

} // end namespace DataTransferKit

//---------------------------------------------------------------------------//

#endif // end DTK_SPLINEINTERPOLATIONPAIRING_IMPL_HPP

//---------------------------------------------------------------------------//
// end DTK_SplineInterpolationPairing_impl.hpp
//---------------------------------------------------------------------------//
