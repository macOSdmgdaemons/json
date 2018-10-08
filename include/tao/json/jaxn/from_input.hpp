// Copyright (c) 2018 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_JAXN_FROM_INPUT_HPP
#define TAO_JSON_JAXN_FROM_INPUT_HPP

#include <cstddef>
#include <utility>

#include "../events/to_value.hpp"
#include "../events/transformer.hpp"

#include "events/from_input.hpp"

namespace tao
{
   namespace json
   {
      namespace jaxn
      {
         template< template< typename... > class Traits, typename Base, template< typename... > class... Transformers, typename... Ts >
         basic_value< Traits, Base > basic_from_input( Ts&&... ts )
         {
            json::events::transformer< json::events::to_basic_value< Traits, Base >, Transformers... > consumer;
            events::from_input( consumer, std::forward< Ts >( ts )... );
            return std::move( consumer.value );
         }

         template< template< typename... > class Traits, template< typename... > class... Transformers, typename... Ts >
         basic_value< Traits > basic_from_input( Ts&&... ts )
         {
            return basic_from_input< Traits, json::internal::empty_base, Transformers... >( std::forward< Ts >( ts )... );
         }

         template< template< typename... > class... Transformers, typename... Ts >
         value from_input( Ts&&... ts )
         {
            return basic_from_input< traits, Transformers... >( std::forward< Ts >( ts )... );
         }

      }  // namespace jaxn

   }  // namespace json

}  // namespace tao

#endif