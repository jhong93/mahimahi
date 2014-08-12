/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "archive.hh"
#include "http_request.hh"
#include "http_response.hh"
#include "http_header.hh"

#include "file_descriptor.hh"
using namespace std;

string remove_query( const string & request_line )
{
    const auto index = request_line.find( "?" );
    if ( index == string::npos ) {
        return request_line;
    } else {
        return request_line.substr( 0, index );
    }
}

int match_size( const string & first, const string & second )
{
    const auto max_match = min( first.size(), second.size() );
    for ( unsigned int i = first.find( "?" ); i < max_match; i++ ) {
        if ( first.at( i ) != second.at( i ) ) {
            return i;
        }
    }
    return max_match;
}

Archive::Archive()
    : archive_()
{}

pair< bool, string > Archive::find_request( const MahimahiProtobufs::HTTPMessage & incoming_req )
{
    /* iterates through requests in the archive and see if request matches any of them. if it does, return <true, response as string>
    otherwise, return <false, ""> */

    pair< int, string > possible_match = make_pair( 0, "" );

    HTTPRequest request( incoming_req );

    for ( auto & x : archive_ ) {
        HTTPRequest curr( x.first );
        if ( remove_query( curr.first_line() ) == remove_query( request.first_line() ) ) { /* up to ? matches */
            if ( curr.get_header_value( "Host" ) == request.get_header_value( "Host" ) ) { /* host header matches */
                if ( curr.first_line() == request.first_line() ) { /* exact match */
                    HTTPResponse ret( x.second );
                    return make_pair( true, ret.str() );
                }
                /* possible match, but not exact */
                int match_val = match_size( curr.first_line(), request.first_line() );
                if ( match_val > possible_match.first ) { /* closer possible match */
                    HTTPResponse ret( x.second );
                    possible_match = make_pair( match_val, ret.str());
                }
            }
        }
    }

    if ( possible_match.first != 0 ) { /* we have a possible match (same object name) */
        return make_pair( true, possible_match.second );
    }

    /* no match */
    return make_pair( false, "" );
}

int Archive::add_request( const MahimahiProtobufs::HTTPMessage & incoming_req )
{
    /* first calls find_request to see if the request is already there. if it is, return -1
    if not, emplace to end of vector as <request, empty HTTPMessage> and return index */

    auto res = find_request( incoming_req );

    if ( res.first == true ) { /* we have request already */
        return -1;
    } else { /* add request to archive */
        MahimahiProtobufs::HTTPMessage blank;
        archive_.emplace_back( make_pair( incoming_req, blank ) );
        return ( archive_.size() - 1 );
    }
}

void Archive::add_response( const MahimahiProtobufs::HTTPMessage & response, const int & index )
{
    /* first assert that response is null at given index. then add response in that position */

    HTTPResponse current_res( archive_.at( index ).second );
    assert( current_res.first_line() == "" );

    archive_.at( index ).second = response;
}

void Archive::print( void )
{
    string bulk_file_name = "archivestuff.txt";
    FileDescriptor bulkreply = SystemCall( "open", open( bulk_file_name.c_str(), O_WRONLY | O_CREAT, 00700 ) );
    string to_write;
    for ( unsigned int i = 0; i < archive_.size(); i++ ) {
        HTTPRequest curr1( archive_.at( i ).first );
        HTTPResponse curr2( archive_.at( i ).second );
        to_write.append( curr1.first_line() + "\n" + curr2.first_line() + "\n\n" );
    }
    bulkreply.write( to_write );
}
