// Copyright (c) 2018 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include <tao/json/external/pegtl.hpp>
#include <tao/json/external/pegtl/contrib/parse_tree.hpp>
#include <tao/json/jaxn/internal/grammar.hpp>

#include <iostream>

using namespace tao::json_pegtl;  // NOLINT

namespace config
{
   namespace jaxn = tao::json::jaxn::internal::rules;

   // clang-format off
   namespace rules
   {
      using ws = plus< jaxn::ws >;

      struct value;
      struct identifier : plus< ranges< 'a', 'z', 'A', 'Z', '0', '9', '-', '-', '_', '_', '$' > > {};

      struct rkey;
      struct function_param : if_must< identifier, jaxn::name_separator, value > {};
      struct function : seq< identifier, ws, list< function_param, jaxn::value_separator > > {};
      struct expression : if_must< string< '$', '(' >, sor< function, rkey >, one< ')' > > {};

      struct string_fragment : sor< expression, jaxn::string_fragment > {};
      struct string : list_must< string_fragment, jaxn::value_concat > {};

      struct binary_fragment : sor< expression, jaxn::bvalue > {};
      struct binary : list_must< binary_fragment, jaxn::value_concat > {};

      struct array_element; // TODO: rename to element
      struct array_content : opt< list_tail< array_element, jaxn::element_separator > > {};
      struct array_value : seq< jaxn::begin_array, array_content, must< jaxn::end_array > >
      {
         using begin = jaxn::begin_array;
         using end = jaxn::end_array;
         using element = array_element;
         using content = array_content;
      };
      struct array_fragment : sor< expression, array_value > {};
      struct array : list_must< array_fragment, jaxn::value_concat > {};

      struct key : string {};

      struct mkey_part : sor< key, identifier > {};
      struct mkey : list< mkey_part, one< '.' > > {};
      struct member : if_must< mkey, jaxn::name_separator, value > {};
      struct object_content : star< member, opt< jaxn::value_separator > > {};
      struct object_value : seq< jaxn::begin_object, object_content, must< jaxn::end_object > >
      {
         using begin = jaxn::begin_object;
         using end = jaxn::end_object;
         using element = member;
         using content = object_content;
      };
      struct object_fragment : sor< expression, object_value > {};
      struct object : list_must< object_fragment, jaxn::value_concat > {};

      struct rkey_part : sor< key, identifier, expression > {};
      struct rkey : seq< star< one< '.' > >, list< mkey_part, one< '.' > > > {};
      struct expression_list : seq< expression, star< jaxn::value_concat, sor< expression, must< sor< string, binary, object, array > > > > > {};

      struct sor_value
      {
         using analyze_t = analysis::generic< analysis::rule_type::SOR, string, jaxn::number< false >, object, array, jaxn::false_, jaxn::true_, jaxn::null >;

         template< typename Rule,
                   apply_mode A,
                   template< typename... > class Action,
                   template< typename... > class Control,
                   typename Input,
                   typename... States >
         static bool match_must( Input& in, States&&... st )
         {
            return Control< must< Rule > >::template match< A, rewind_mode::DONTCARE, Action, Control >( in, st... );
         }

         template< bool NEG,
                   apply_mode A,
                   rewind_mode M,
                   template< typename... > class Action,
                   template< typename... > class Control,
                   typename Input,
                   typename... States >
         static bool match_zero( Input& in, States&&... st )
         {
            if( in.size( 2 ) > 1 ) {
               switch( in.peek_char( 1 ) ) {
                  case '.':
                  case 'e':
                  case 'E':
                     return Control< jaxn::number< NEG > >::template match< A, M, Action, Control >( in, st... );

                  case 'x':
                  case 'X':
                     in.bump_in_this_line( 2 );
                     return Control< must< jaxn::hexnum< NEG > > >::template match< A, M, Action, Control >( in, st... );

                  case '0':
                  case '1':
                  case '2':
                  case '3':
                  case '4':
                  case '5':
                  case '6':
                  case '7':
                  case '8':
                  case '9':
                     throw parse_error( "invalid leading zero", in );
               }
            }
            in.bump_in_this_line();
            // TODO: Control< jaxn::zero< NEG > >::template apply0< Action >( in, st... );
            return true;
         }

         template< bool NEG,
                   apply_mode A,
                   rewind_mode M,
                   template< typename... > class Action,
                   template< typename... > class Control,
                   typename Input,
                   typename... States >
         static bool match_number( Input& in, States&&... st )
         {
            switch( in.peek_char() ) {
               case 'N':
                  return Control< must< jaxn::nan > >::template match< A, M, Action, Control >( in, st... );

               case 'I':
                  return Control< must< jaxn::infinity< NEG > > >::template match< A, M, Action, Control >( in, st... );

               case '0':
                  if( !match_zero< NEG, A, rewind_mode::DONTCARE, Action, Control >( in, st... ) ) {
                     throw parse_error( "incomplete number", in );
                  }
                  return true;

               default:
                  return Control< jaxn::number< NEG > >::template match< A, M, Action, Control >( in, st... );
            }
         }

         template< apply_mode A,
                   rewind_mode M,
                   template< typename... > class Action,
                   template< typename... > class Control,
                   typename Input,
                   typename... States >
         static bool match_number_or_date_time( Input& in, States&&... st )
         {
            if( in.size( 5 ) >= 5 && std::isdigit( in.peek_char() ) && std::isdigit( in.peek_char( 1 ) ) ) {
               const auto c = in.peek_char( 2 );
               if( c == ':' ) {
                  return Control< jaxn::local_time >::template match< A, M, Action, Control >( in, st... );
               }
               if( std::isdigit( c ) && std::isdigit( in.peek_char( 3 ) ) && ( in.peek_char( 4 ) == '-' ) ) {
                  return Control< jaxn::date_sequence >::template match< A, M, Action, Control >( in, st... );
               }
            }
            return match_number< false, A, M, Action, Control >( in, st... );
         }

         template< apply_mode A,
                   rewind_mode M,
                   template< typename... > class Action,
                   template< typename... > class Control,
                   typename Input,
                   typename... States >
         static bool match_impl( Input& in, States&&... st )
         {
            switch( in.peek_char() ) {
               case '{': return Control< object >::template match< A, M, Action, Control >( in, st... );
               case '[': return Control< array >::template match< A, M, Action, Control >( in, st... );
               case 'n': return Control< jaxn::null >::template match< A, M, Action, Control >( in, st... );
               case 't': return Control< jaxn::true_ >::template match< A, M, Action, Control >( in, st... );
               case 'f': return Control< jaxn::false_ >::template match< A, M, Action, Control >( in, st... );

               case '"':
               case '\'':
                  return Control< string >::template match< A, M, Action, Control >( in, st... );

               case '$':
                  if( in.peek_char( 1 ) == '(' ) {
                     return Control< expression_list >::template match< A, M, Action, Control >( in, st... );
                  }
                  else {
                     return Control< binary >::template match< A, M, Action, Control >( in, st... );
                  }

               case '+':
                  in.bump_in_this_line();
                  if( in.empty() || !match_number< false, A, rewind_mode::DONTCARE, Action, Control >( in, st... ) ) {
                     throw parse_error( "incomplete number", in );
                  }
                  return true;

               case '-':
                  in.bump_in_this_line();
                  if( in.empty() || !match_number< true, A, rewind_mode::DONTCARE, Action, Control >( in, st... ) ) {
                     throw parse_error( "incomplete number", in );
                  }
                  return true;

               default:
                  return match_number_or_date_time< A, M, Action, Control >( in, st... );
            }
         }

         template< apply_mode A,
                   rewind_mode M,
                   template< typename... > class Action,
                   template< typename... > class Control,
                   typename Input,
                   typename... States >
         static bool match( Input& in, States&&... st )
         {
            if( in.size( 2 ) && match_impl< A, M, Action, Control >( in, st... ) ) {
               in.discard();
               return true;
            }
            return false;
         }
      };

      struct value : jaxn::padr< sor_value > {};
      struct array_element : value {};

      struct kw_include : TAO_JSON_PEGTL_STRING( "include" ) {};
      struct kw_delete : TAO_JSON_PEGTL_STRING( "delete" ) {};

      struct include_file : seq< kw_include, ws, string > {};
      struct delete_keys : seq< kw_delete, ws, mkey > {};

      struct statement : sor< include_file, delete_keys, member > {};

      struct grammar : until< eof, sor< ws, must< rules::statement > > > {};
      // clang-format on

   }  // namespace rules

   template< typename Rule >
   using selector = parse_tree::selector<
      Rule,
      parse_tree::apply_remove_content::to<
         rules::include_file,
         rules::delete_keys,
         rules::member,
         rules::string,
         rules::array,
         rules::object,
         rules::function,
         rules::function_param,
         rules::expression,
         rules::rkey,
         rules::binary,
         rules::array_element,
         jaxn::infinity< true >,
         jaxn::infinity< false >,
         jaxn::null,
         jaxn::true_,
         jaxn::false_,
         jaxn::nan >,
      parse_tree::apply_fold_one::to<
         rules::mkey >,
      parse_tree::apply_store_content::to<
         rules::identifier,
         jaxn::local_time,
         jaxn::date_sequence,
         jaxn::bvalue,
         jaxn::number< true >,
         jaxn::number< false >,
         jaxn::hexnum< true >,
         jaxn::hexnum< false >,
         jaxn::string_fragment > >;

   void print_node( const parse_tree::node& n, const std::string& s = "" )
   {
      // detect the root node:
      if( n.is_root() ) {
         std::cout << "ROOT" << std::endl;
      }
      else {
         if( n.has_content() ) {
            std::cout << s << n.name() << " \"" << n.content() << "\" at " << n.begin() << " to " << n.end() << std::endl;
         }
         else {
            std::cout << s << n.name() << " at " << n.begin() << std::endl;
         }
      }
      // print all child nodes
      if( !n.children.empty() ) {
         const auto s2 = s + "  ";
         for( auto& up : n.children ) {
            print_node( *up, s2 );
         }
      }
   }

}  // namespace config

int main( int argc, char** argv )
{
   for( int i = 1; i < argc; ++i ) {
      file_input<> in( argv[ i ] );
      const auto root = parse_tree::parse< config::rules::grammar, config::selector >( in );  // TODO: Add error_control
      config::print_node( *root );
   }
   return 0;
}
