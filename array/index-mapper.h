#pragma once

DECLARE_NAMESPACE_NLL

namespace details
{
   /// helper to compute the strides for index mapper of a multislice memory block
   template <size_t N, size_t Z_INDEX_>
   struct Mapper_multisplice_stride_row_major
   {
   public:
      using Vectorui = core::StaticVector<ui32, N>;

      Vectorui operator()( const Vectorui& shape ) const
      {
         Vectorui strides;
         ui32 stride = 1;
         for ( size_t n = 0; n < N; ++n )
         {
            if ( n != Z_INDEX_ )
            {
               strides[ n ] = stride;
               stride *= shape[ n ];
            }
         }
         return strides;
      }
   };

   /// helper to compute the strides for index mapper of a contiguous memory block, row major
   template <size_t N>
   struct Mapper_stride_row_major
   {
   public:
      using Vectorui = core::StaticVector<ui32, N>;

      Vectorui operator()( const Vectorui& shape ) const
      {
         Vectorui strides;
         ui32 stride = 1;
         for ( size_t n = 0; n < N; ++n )
         {
            strides[ n ] = stride;
            stride *= shape[ n ];
         }
         return strides;
      }
   };

   /// helper to compute the strides for index mapper of a contiguous memory block, column major
   template <size_t N>
   struct Mapper_stride_column_major
   {
   public:
      using Vectorui = core::StaticVector<ui32, N>;

      Vectorui operator()( const Vectorui& shape ) const
      {
         Vectorui strides;

         ui32 stride = 1;
         for ( int n = N - 1; n >= 0; --n )
         {
            strides[ n ] = stride;
            stride *= shape[ n ];
         }
         return strides;
      }
   };
}

/**
@brief memory will be stored linearly (can be expressed as origin + x * strides within slices)
*/
struct memory_layout_linear
{};

/**
@brief memory will be stored contiguously
*/
struct memory_layout_contiguous : public memory_layout_linear
{};

/**
@brief memory will be stored contiguously EXCEPT in the last dimensions.

E.g., for Dim=3, (x, y, ...) are stored in contiguous portions of memory,
but not (..., ..., n) and (..., ..., n+1)
*/
struct memory_layout_multislice_z : public memory_layout_linear
{};

/**
@brief index mapper for multislice z memory layout.
*/
template < size_t N, class Mapper = details::Mapper_stride_row_major<N>>
class IndexMapper_contiguous : public memory_layout_contiguous
{
public:
   using Vectorui = core::StaticVector<ui32, N>;
   using IndexMapper = IndexMapper_contiguous<N, Mapper>;

   /**
   @param shape the size of the area to map
   @param physical_strides the strides of the area to map, considering the underlying memory (i.e,
   if we reference a sub-block, this must be factored in the stride)
   */
   void init( ui32 origin, const Vectorui& shape )
   {
      Vectorui strides = Mapper()( shape );

      _origin = origin;
      _physicalStrides = strides;
   }

   ui32 offset( const Vectorui& index ) const
   {
      return core::dot( index, _physicalStrides ) + _origin;
   }

   IndexMapper submap( const Vectorui& origin, const Vectorui& shape, const Vectorui& strides ) const
   {
      const auto newPhysicalStrides = _physicalStrides * strides;
      const auto newOrigin = offset( origin );

      IndexMapper newMapper;
      newMapper.init( newOrigin, shape );
      newMapper._physicalStrides = newPhysicalStrides;
      return newMapper;
   }

   // this is not part of the generic interface but can be specifically used for a memory/mapper
   const Vectorui& _getPhysicalStrides() const
   {
      return _physicalStrides;
   }

private:
   ui32     _origin;
   Vectorui _physicalStrides;
};

template <class Mapper>
class IndexMapper_contiguous_matrix : public IndexMapper_contiguous<2, Mapper>
{};

template < size_t N>
using IndexMapper_contiguous_row_major = IndexMapper_contiguous<N, details::Mapper_stride_row_major<N>>;

template < size_t N>
using IndexMapper_contiguous_column_major = IndexMapper_contiguous<N, details::Mapper_stride_column_major<N>>;

/**
@brief index mapper for row major matrices

We have a special mapper for matrix and use this to differentiate between Matrix and
array. Else an array <float, 2> will be considered a matrix
*/
using IndexMapper_contiguous_matrix_row_major = IndexMapper_contiguous_matrix<details::Mapper_stride_column_major<2>>;

/**
@brief index mapper for column major matrices

We have a special mapper for matrix and use this to differentiate between Matrix and
array. Else an array <float, 2> will be considered a matrix
*/
using IndexMapper_contiguous_matrix_column_major = IndexMapper_contiguous_matrix<details::Mapper_stride_row_major<2>>;

/**
@brief index mapper for multislice z memory layout.
*/
template <size_t N, size_t Z_INDEX_, class Mapper = details::Mapper_multisplice_stride_row_major<N, Z_INDEX_>>
class IndexMapper_multislice : public memory_layout_multislice_z
{
public:
   using Vectorui = core::StaticVector<ui32, N>;
   static const size_t Z_INDEX = Z_INDEX_;
   using IndexMapper = IndexMapper_multislice<N, Z_INDEX, Mapper>;

   /**
   @param shape the size of the area to map
   @param physical_strides the strides of the area to map, considering the underlying memory (i.e,
   if we reference a sub-block, this must be factored in the stride)
   */
   void init( ui32 origin, const Vectorui& shape )
   {
      Vectorui strides = Mapper()( shape );

      _origin = origin;
      _physicalStrides = strides;
      _physicalStrides[ Z_INDEX ] = 0;   // in slice offset should be independent of Z_INDEX axis
   }

   ui32 offset( const Vectorui& index ) const
   {
      return core::dot( index, _physicalStrides ) + _origin;
   }

   IndexMapper submap( const Vectorui& origin, const Vectorui& shape, const Vectorui& strides ) const
   {
      const auto newPhysicalStrides = _physicalStrides * strides;
      const auto newOrigin = offset( origin );

      IndexMapper newMapper;
      newMapper.init( newOrigin, shape );
      newMapper._physicalStrides = newPhysicalStrides;
      return newMapper;
   }

public:
   // this is not part of the generic interface but can be specifically used for a memory/mapper
   const Vectorui& _getPhysicalStrides() const
   {
      return _physicalStrides;
   }

private:
   ui32     _origin;
   Vectorui _physicalStrides;
};
  
DECLARE_NAMESPACE_END