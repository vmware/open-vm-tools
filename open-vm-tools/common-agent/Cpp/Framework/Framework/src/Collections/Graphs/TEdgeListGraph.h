/*
 *  Created on: Jul 09, 2003
 *      Author: Scott VanCamp
 *
 *  Copyright (C) 2003-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef TEDGELISTGRAPH_H_
#define TEDGELISTGRAPH_H_

#include <set>
#include "Exception/CCafException.h"
#include <map>
#include <list>
#include <algorithm>

//////////////////////////////////////////////////////////////////////////////
//
// Simple graph that uses an edge list
//
// Since we may be using this graph with smart pointers I allow you to specify
// you own less than function. See <EcmCommonStaticMinDep\TEcmSmartClFunctions.h>
// for smart class less than template functions.
//////////////////////////////////////////////////////////////////////////////
using namespace Caf;

template < class Item, class CustomLess = std::less< Item > >
class TEdgeListGraph
{
	private:
		typedef TEdgeListGraph< Item, CustomLess > SameGraphType;

	public:
		typedef std::set< Item, CustomLess> CsetVertexEdges;
		typedef std::set< Item* > CsetptrVertexEdges;
		typedef std::map< Item, CsetptrVertexEdges, CustomLess > CmapVertexEdgeList;
		typedef std::list< Item > ClistVertexEdges;

		TEdgeListGraph()
			: CAF_CM_INIT( "TEdgeListGraph" )
		{
		}

		Item findVertex( const Item& crItemVertex ) const
		{
			return internalFindVertex(crItemVertex, _mapEdgeList);
		}

		bool isVertexInGraph( const Item& crItemVertex ) const
		{
			return internalIsVertexInGraph(crItemVertex, _mapEdgeList);
		}

		bool isEdgeInGraph(const Item& source, const Item& destination) const
		{
			return internalIsEdgeInGraph(source, destination, _mapEdgeList);
		}

		void addVertex( const Item& crItemVertex )
		{
			internalAddVertex(crItemVertex, _mapEdgeList);
		}

		void removeVertices( const CsetVertexEdges& crsetParents)
		{
			internalRemoveVertices(crsetParents, _mapEdgeList);
		}

		void removeVertex( const Item& crItemVertex )
		{
			return internalRemoveVertex(crItemVertex, _mapEdgeList);
		}

		void addEdge( const Item& crItemSource, const Item& crItemDestination )
		{
			internalAddEdge(crItemSource, crItemDestination, _mapEdgeList);
		}

		void removeEdge( const Item& crItemSource, const Item& crItemDestination )
		{
			internalRemoveEdge(crItemSource, crItemDestination, _mapEdgeList);
		}

		CsetVertexEdges getVertices() const
		{
			return internalGetVertices(_mapEdgeList);
		}

		uint32 getNumberVertices() const
		{
			return static_cast<uint32>(_mapEdgeList.size());
		}

		//////////////////////////////////////////////////////////////////////
		//
		//  getEdges()
		//
		//  Does not recurse into the dependencies, returns the immediate items
		//  that the item depends on.
		//////////////////////////////////////////////////////////////////////
		CsetVertexEdges getEdges( const Item& crItemVertex ) const
		{
			return internalGetEdges(crItemVertex, _mapEdgeList);
		}

		//////////////////////////////////////////////////////////////////////
		//
		//  getParents()
		//
		//	Returns the parents of a given vertex
		//
		//////////////////////////////////////////////////////////////////////
		CsetVertexEdges getParents( const Item& crItemVertex ) const
		{
			return internalGetParents(crItemVertex, _mapEdgeList);
		}

		//////////////////////////////////////////////////////////////////////
		//
		//  getAllParents()
		//	 Returns a set of all the parents of a particular vertex
		//
		//////////////////////////////////////////////////////////////////////
		CsetVertexEdges getAllParents( const Item& crItemVertex ) const
		{
			CsetVertexEdges oSetParents;

			// Get the Parents
			internalGetAllParents( crItemVertex, _mapEdgeList, oSetParents );
			return oSetParents;
		}

		void copyGraph(SameGraphType& copy) const
		{
			internalCopyGraph(*this, copy);
		}

		//////////////////////////////////////////////////////////////////////
		//
		// topologySort()
		//	Returns a topology sort of the entire graph.
		// The returned list indicates vertices in sorted order
		//
		//////////////////////////////////////////////////////////////////////
		// Kahn, A. B. (1962), "Topological sorting of large networks", Communications of the ACM 5 (11): 558�562
		//
		//		L <- Empty list that will contain the sorted elements
		//		S <- Set of all nodes with no incoming edges
		//		while S is non-empty do
		//		    remove a node n from S
		//		    insert n into L
		//		    for each node m with an edge e from n to m do
		//		        remove edge e from the graph
		//		        if m has no other incoming edges then
		//		            insert m into S
		//		if graph has edges then
		//		    output error message (graph has at least one cycle)
		//		else
		//		    output message (proposed topologically sorted order: L)
		ClistVertexEdges topologySort() const
		{
			CAF_CM_FUNCNAME("topologySort");
			ClistVertexEdges sortedList;

			// Work with a temporary copy of the graph
			TEdgeListGraph graphCopy;
			internalCopyGraph(*this, graphCopy);
			CsetVertexEdges setNoParents;

			CsetVertexEdges vertices = graphCopy.getVertices();
			for (typename CsetVertexEdges::const_iterator vertex = vertices.begin();
					vertex != vertices.end();
					++vertex) {
				if (graphCopy.getParents(*vertex).size() == 0) {
					setNoParents.insert(*vertex);
				}
			}

			while (setNoParents.size()) {
				Item n = *(setNoParents.begin());
				setNoParents.erase(setNoParents.begin());
				sortedList.push_back(n);
				CsetVertexEdges edges = graphCopy.getEdges(n);
				for (typename CsetVertexEdges::const_iterator edge = edges.begin();
						edge != edges.end();
						++edge) {
					graphCopy.removeEdge(n, *edge);

					bool edgeFound = false;
					CsetVertexEdges vertices = graphCopy.getVertices();
					for (typename CsetVertexEdges::const_iterator vertex = vertices.begin();
							!edgeFound && (vertex != vertices.end());
							++vertex) {
						edgeFound = graphCopy.isEdgeInGraph(*vertex, *edge);
					}

					if (!edgeFound) {
						setNoParents.insert(*edge);
					}
				}
			}

			// Cycle check
			vertices = graphCopy.getVertices();
			for (typename CsetVertexEdges::const_iterator vertex = vertices.begin();
					vertex != vertices.end();
					++vertex) {
				if (graphCopy.getEdges(*vertex).size()) {
					CAF_CM_EXCEPTION_VA0(ERROR_INVALID_STATE, "The graph has at least one cycle");
				}
			}

			return sortedList;
		}

	private:
		Item internalFindVertex( const Item& crItemVertex, const CmapVertexEdgeList& graph ) const
		{
			Item FoundVertex;
			// Find the vertex
			typename CmapVertexEdgeList::const_iterator imapVertex = graph.find( crItemVertex );
			if(  imapVertex != graph.end() )
			{
				FoundVertex = ( imapVertex->first );
			}
			return FoundVertex;
		}

		bool internalIsVertexInGraph( const Item& crItemVertex, const CmapVertexEdgeList& graph ) const
		{
			bool bVertexIsInGraph = false;

			// Find the vertex
			typename CmapVertexEdgeList::const_iterator imapVertex = graph.find( crItemVertex );
			if(  imapVertex != graph.end() )
			{
				bVertexIsInGraph = true;
			}
			return bVertexIsInGraph;
		}

		bool internalIsEdgeInGraph(const Item& source, const Item& destination, const CmapVertexEdgeList& graph) const
		{
			CsetVertexEdges edges = internalGetEdges(source, graph);
			return (edges.end() != edges.find(destination));
		}


		void internalAddVertex( const Item& crItemVertex, CmapVertexEdgeList& graph )
		{
			CAF_CM_FUNCNAME("internalAddVertex");
			// Ensure that the Vertex doesn't already exist in the graph
			// we do not allow duplicate Vertices to exist.
			typename CmapVertexEdgeList::const_iterator imapEdgeList
					=  graph.find( crItemVertex );
			if ( graph.end() == imapEdgeList )
			{
				graph.insert( std::make_pair( crItemVertex, CsetptrVertexEdges() ) );
			}
			else
			{
				CAF_CM_EXCEPTION_VA0(
						ERROR_DUPLICATE_TAG,
						"Vertex already exists in the graph, cannot add duplicate Vertices");
			}
		}

		void internalRemoveVertices( const CsetVertexEdges& crsetParents, CmapVertexEdgeList& graph)
		{
			for( typename CsetVertexEdges::const_iterator isetVertex = crsetParents.begin();
				 isetVertex != crsetParents.end();
				 ++isetVertex )
			{
				internalRemoveVertex( *isetVertex, graph );
			}
		}

		void internalRemoveVertex( const Item& crItemVertex, CmapVertexEdgeList& graph )
		{
			CsetVertexEdges osetVerticesAffected;

			// Find the vertex
			typename CmapVertexEdgeList::iterator imapVertex = graph.find( crItemVertex );
			if(  imapVertex != graph.end() )
			{
				// Get an Item pointer to the vertex that we wish to remove
				Item* pItem = const_cast<Item*>( &( imapVertex->first ) );

				// Iterate over each vertex removing each reference to the vertex being removed
				for( typename CmapVertexEdgeList::iterator imapEdgeList =  graph.begin();
					 imapEdgeList != graph.end();
					 ++imapEdgeList )
				{
					// Is the item in the edge list for the current vertex
					typename CsetptrVertexEdges::iterator isetVertex = imapEdgeList->second.find( pItem );
					if( isetVertex != imapEdgeList->second.end() )
					{
						// Remove the reference
						imapEdgeList->second.erase( isetVertex );
					}
				}

				// We no longer need the Item pointer
				pItem = NULL;

				// Remove the vertex
				graph.erase( imapVertex );
			}
		}

		void internalAddEdge( const Item& crItemSource, const Item& crItemDestination, CmapVertexEdgeList& graph )
		{
			CAF_CM_FUNCNAME("internalAddEdge");

			// Ensure that you are not trying to add a dependency to yourself
			if ( crItemSource == crItemDestination )
			{
				CAF_CM_EXCEPTION_VA0(
						ERROR_INVALID_DATA,
						"Edges to yourself are not allowed, cannot add edge");
			}

			// Ensure that there is a source vertex
			typename CmapVertexEdgeList::iterator imapSourceVertex = graph.find( crItemSource );
			if(  imapSourceVertex == graph.end() )
			{
				CAF_CM_EXCEPTION_VA0(
						ERROR_TAG_NOT_FOUND,
						"Unable to find source vertex, cannot add edge");
			}

			// Ensure that there is a destination vertex
			typename CmapVertexEdgeList::iterator imapDestinationVertex = graph.find( crItemDestination );
			if(  imapDestinationVertex == graph.end() )
			{
				CAF_CM_EXCEPTION_VA0(
						ERROR_TAG_NOT_FOUND,
						"Unable to find destination vertex, cannot add edge");
			}

			// Create the edge
			Item* pItem = const_cast<Item*>( &( imapDestinationVertex->first ) );
			imapSourceVertex->second.insert( pItem );
		}

		void internalRemoveEdge( const Item& crItemSource, const Item& crItemDestination, CmapVertexEdgeList& graph )
		{
			// Ensure that there is a source vertex
			typename CmapVertexEdgeList::iterator imapSourceVertex = graph.find( crItemSource );
			if(  imapSourceVertex != graph.end() )
			{
				// Ensure that there is a destination vertex
				typename CmapVertexEdgeList::iterator imapDestinationVertex = graph.find( crItemDestination );
				if(  imapDestinationVertex != graph.end() )
				{
					// Get a pointer to the edge
					Item* pItem = const_cast<Item*>( &( imapDestinationVertex->first ) );

					// remove the edge
					imapSourceVertex->second.erase( pItem );
				}
			}
		}

		CsetVertexEdges internalGetVertices(const CmapVertexEdgeList& graph) const
		{
			CsetVertexEdges osetVertices;
			for( typename CmapVertexEdgeList::const_iterator imapEdgeList =  graph.begin();
				 imapEdgeList != graph.end();
				 ++imapEdgeList )
			{
				osetVertices.insert( imapEdgeList->first );
			}
			return osetVertices;
		}

		CsetVertexEdges internalGetEdges( const Item& crItemVertex, const CmapVertexEdgeList& graph ) const
		{
			CAF_CM_FUNCNAME("getEdges");
			CsetVertexEdges osetEdges;

			// Find the vertex
			typename CmapVertexEdgeList::const_iterator imapVertex = graph.find( crItemVertex );
			if(  imapVertex == graph.end() )
			{
				CAF_CM_EXCEPTION_VA0(
						ERROR_TAG_NOT_FOUND,
						"Unable to find vertex. Cannot get edges");
			}
			else
			{
				for( typename CsetptrVertexEdges::const_iterator isetEdge = imapVertex->second.begin();
					 isetEdge != imapVertex->second.end();
					 ++isetEdge )
				{
					if( *isetEdge == NULL )
					{
						CAF_CM_EXCEPTION_VA0(
								E_FAIL,
								"Invalid graph, an edge reported pointing to a invalid destination vertex");
					}
					osetEdges.insert( **isetEdge );
				}
			}
			return osetEdges;
		}

		CsetVertexEdges internalGetParents(
				const Item& crItemVertex,
				const CmapVertexEdgeList& graph) const
		{
			CsetVertexEdges oSetParents;

			// Find the vertex
			typename CmapVertexEdgeList::const_iterator cimapVertex = graph.find( crItemVertex );
			if(  cimapVertex != graph.end() )
			{
				// Get an Item pointer to the vertex that we wish to get parents for
				Item* pItem = const_cast<Item*>( &( cimapVertex->first ) );

				// Iterate over each vertex in the graph
				for( typename CmapVertexEdgeList::const_iterator cimapEdgeList =  graph.begin();
					 cimapEdgeList != graph.end();
					 ++cimapEdgeList )
				{
					// Is the item in the edge list for the current vertex
					typename CsetptrVertexEdges::const_iterator cisetVertex = cimapEdgeList->second.find( pItem );
					if( cisetVertex != cimapEdgeList->second.end() )
					{
						// Add the vertex to the set of parents
						oSetParents.insert( cimapEdgeList->first );
					}
				}

				// We no longer need the Item pointer
				pItem = NULL;
			}
			return oSetParents;
		}

		void internalGetAllParents(
				const Item& crItemVertex,
				const CmapVertexEdgeList& graph,
				CsetVertexEdges& roSetParents) const
		{
			// Find the vertex in the map
			typename CmapVertexEdgeList::const_iterator imapVertex = graph.find( crItemVertex );
			if(  imapVertex != graph.end() )
			{
				// Get the parents of this vertex
				CsetVertexEdges setCurrentParents = internalGetParents( crItemVertex, graph );

				// For each parent found that we have not recorded yet find its parents
				for( typename CsetVertexEdges::iterator isetCurrentParent = setCurrentParents.begin();
					 isetCurrentParent != setCurrentParents.end();
					 ++isetCurrentParent )
				{
					// Have we recorded this current parent in the parent set?
					typename CsetVertexEdges::iterator isetIsCurrentParentRecorded = roSetParents.find( *isetCurrentParent );
					if( isetIsCurrentParentRecorded == roSetParents.end() )
					{
						// Since we have not recorded this vertex, record it and get its parents
						roSetParents.insert( *isetCurrentParent );

						// Since we have not recorded this parent, record it and get its parents
						internalGetAllParents(*isetCurrentParent, graph, roSetParents );
					}
				}
			}
		}

		void internalCopyGraph(const SameGraphType& source, SameGraphType& copy) const
		{
			CsetVertexEdges sourceVertices = source.getVertices();
			for (typename CsetVertexEdges::const_iterator vertex = sourceVertices.begin();
					vertex != sourceVertices.end();
					++vertex) {
				copy.addVertex(*vertex);
			}

			for (typename CsetVertexEdges::const_iterator vertex = sourceVertices.begin();
					vertex != sourceVertices.end();
					++vertex) {
				CsetVertexEdges sourceEdges = source.getEdges(*vertex);
				for (typename CsetVertexEdges::const_iterator edge = sourceEdges.begin();
						edge != sourceEdges.end();
						++edge) {
					copy.addEdge(*vertex, *edge);
				}
			}
		}

		CmapVertexEdgeList  _mapEdgeList;
		CAF_CM_CREATE;
};

#endif /* TEDGELISTGRAPH_H_ */
