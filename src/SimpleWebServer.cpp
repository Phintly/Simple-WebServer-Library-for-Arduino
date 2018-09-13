// Copyright  : Dennis Buis (2017)
// License    : MIT
// Platform   : Arduino
// Library    : Simple WebServer Library for Arduino & ESP8266
// File       : SimpleWebServer.cpp
// Purpose    : Arduino library to create a simple webserver responding to API calls
// Repository : https://github.com/DennisB66/Simple-WebServer-Library-for-Arduino

#include <Arduino.h>
#include "SimpleWebServer.h"
#include "SimpleUtils.h"

#define SERVER_METH_READ  0                                 // state engine values
#define SERVER_METH_DONE  1
#define SERVER_PATH_INIT  2
#define SERVER_PATH_READ  3
#define SERVER_ARGS_INIT  4
#define SERVER_ARGS_READ  5
#define SERVER_ARGS_NEXT  6
#define SERVER_HTTP_INIT  7
#define SERVER_HTTP_READ  8
#define HTTP_REQUEST_ERR  9

int returnCode = 400;                                       // HTTP response code (default = ERROR)

// create Webserver task (for a specfic method)
SimpleWebServerTask::SimpleWebServerTask( TaskFunc func, const char* device, HTTPMethod method)
: SimpleTask( func)
, _device( NULL)
, _method( method)
{
  if ( device) {                                            // create device string
    _device = (char*)  malloc( sizeof( char) * ( strlen( device) + 1));
    strcpy( _device, device);
  }
}

// remove Webserver task
SimpleWebServerTask::~SimpleWebServerTask()
{
  if ( _device) free( _device);                             // destroy device string
}

// return targeted device
const char* SimpleWebServerTask::device()
{
  return _device;
}

// return targeted method
HTTPMethod SimpleWebServerTask::method()
{
  return _method;
}

// create server instance (default port = 80)
SimpleWebServer::SimpleWebServer( int port)
: SimpleTaskList()
, _port  ( port)
, _server( port)
{
}

// start webserver
void SimpleWebServer::begin()
{
  _server.begin();                                          // ethernet server begin
}

// return server port number
int SimpleWebServer::port()
{
  return _port;                                             // return server port number
}

// check on incoming connection (true = HTTP request received)
bool SimpleWebServer::connect()
{
  _client = _server.available();

  if ( _client) {                                           // incoming client request
    int i = 0;                                              // reset buffer

    while ( _client.connected()) {                          // check on active client session
      if ( _client.available()) {                           // check on available client data
        char c = _client.read();                            // read client data (char by char)

        if (( c == '\r') || (c == '\n')) {                  // check on end of HTTP request
          break;                                            // HTTP request complete
        }

        _buffer[i++] = c;                                   // add char to buffer
      }
    }

#ifdef SIMPLEWEBSERVER_DEBUG
    VALUE( _buffer) LF;                                     // print full HTTP request"
#endif

    if ( _parseRequest()) {                                 // breakdown HTTP request
      if (( _pathCount) || ( _argsCount)) {                 // HTTP request
        return true;
      } else {
        response( HTTP_OK);                                 // HTTP identify
      }
    } else {
      response( 400);                                       // HTTP error
    }
  }

  return false;                                             // success: ready for further processing
}

void SimpleWebServer::disconnect()
{
  if ( _client.connected()) { delay(10); _client.stop(); }  // terminate client session if still open
}

// send response (code = 200 OK)
void SimpleWebServer::response( int code)
{
  _sendHeader( code);                                       // send header with response code
}

// send response (code, content type)
void SimpleWebServer::response( int code, const char* content_type)
{
  _sendHeader( code, 0, content_type);                      // send header with response code + content type
}

// send response (code, content type, content)
void SimpleWebServer::response( int code, const char* content_type, const char* content)
{
  _sendHeader( code, strlen( content) + 2, content_type);   // send header with response code + content type
  _sendContent( content);                                   // send content (e.g. JSON)
}
// send response (code, content type, FLASH content)
void SimpleWebServer::response( int code, const char* content_type, __FlashStringHelper* content)
{
  _sendHeader( code, 0, content_type);                      // send header with response code + content type
  _sendContent( content);                                   // send content (e.g. JSON)
}

// attach callback function (callback, device, method)
void  SimpleWebServer::handleOn( TaskFunc func, const char* name, HTTPMethod method)
{
  SimpleWebServerTask* task = new SimpleWebServerTask( func, name, method);
                                                            // create new webserver task
  _attach( task);                                           // attach task to list
}

// route incoming requests to the proper callback function
void SimpleWebServer::handle()
{
  SimpleWebServerTask* task = (SimpleWebServerTask*) _rootTask;
                                                            // start with first task
  TaskFunc             func;                                // active callback function

  returnCode = 400;                                         // default return code = error

  if ( connect()) {                                         // request available from client
    while ( task != NULL) {                                 // if list contains tasks
      func = task->func();                                  // get callback function
      if ( method( task->method()) && path( 0, task->device())) {
                                                            // check targeted method & targeted device
        (*func)();                                          // execute callback function
      }

      task = (SimpleWebServerTask*) task->next();           // goto next task
    }

  response( returnCode);
  disconnect();                                             // end of client session
  }
}

// return full HTTP request
char* SimpleWebServer::request()
{
  return _buffer;                                           // return HTTP request
}

// return method of HTTP request
HTTPMethod SimpleWebServer::method()
{
  return _method;                                           // return HTTP method
}

// true = active method equals targeted method
bool SimpleWebServer::method( HTTPMethod method)
{
  return (( method == HTTP_ANY) || ( method == _method));   // verify valid method
}

// return number of (recognized) path items
int SimpleWebServer::pathCount()
{
  return _pathCount;                                        // return number of items in path array
}

// returns value of HTTP path at index i
const char* SimpleWebServer::path( uint8_t i)
{
  return ( i < _pathCount) ? _path[ i] : NULL;
}                                                           // return path item at index i

// checks if pathItem exists at index i
bool SimpleWebServer::path( uint8_t i, const char* pathItem)
{
  return ( i < _pathCount) ? strCmp( pathItem, _path[ i]) : false;
}                                                           // return true if pathItem exists

// return number of (recognized) arguments
int SimpleWebServer::argsCount()
{
  return _argsCount;                                        // return number of items in args array
}

// return value of argument with a specfic label
const char* SimpleWebServer::arg( const char* label)
{
  for ( int i = 0; i < _argsCount; i++) {                   // for all args items
    if ( strCmp( _args[ i].label, label)) {                 // if label exists
      return _args[ i].value ?  _args[ i].value : _args[ i].label;
    }                                                      // return value at index i
  }

  return NULL;
}

// return true if value of argument with a specfic label exists
bool SimpleWebServer::arg( const char* label, const char* value)
{
  bool found = false;

  for ( int i = 0; i < _argsCount; i++) {                   // for all args items
    found |= (( strCmp( _args[i].label, label)) &&
              ( strCmp( _args[i].value, value)));           // return true if value exists at index i
  }

  return found;
}

// break down HTTP request (e.g. "GET /path/1?arg1=0&arg2=1 HTTP/1.1")
bool SimpleWebServer::_parseRequest()
{
  char* meth = _buffer;                                     // HTTP method  string
  char* http = _buffer;                                     // HTTP version string
  int   mode = SERVER_METH_READ;                            // state engine value
  int   leng = strlen( _buffer);                            // length of HTTP request
  bool  _ok_ = true;

  _pathCount = 0;                                           // reset number of path items
  _argsCount = 0;                                           // reset number of argument items

  for ( int i = 0; i < leng; i++) {                         // buffer loop
    switch ( mode) {
      case SERVER_METH_READ :                               // read HTTP _method
      if ( _buffer[i] == ' ') { _buffer[i] = 0; mode = SERVER_METH_DONE; }
      break;

      case SERVER_METH_DONE :                               // prep for first path item
      if ( _buffer[i] == '/') { _buffer[i] = 0; mode = SERVER_PATH_INIT; }
      break;

      case SERVER_PATH_INIT :                               // init next part item
      if ( _buffer[i] == ' ') { _buffer[i] = 0; mode = HTTP_BAD_REQUEST; break; }
      _path[ _pathCount++] = _buffer + i;
      mode = SERVER_PATH_READ;                              // no break = include current char in path read

      case SERVER_PATH_READ :                               // read next path item
      if ( _buffer[i] == '/') { _buffer[i] = 0; mode = SERVER_PATH_INIT; }
      if ( _buffer[i] == '?') { _buffer[i] = 0; mode = SERVER_ARGS_INIT; }
      if ( _buffer[i] == ' ') { _buffer[i] = 0; mode = SERVER_HTTP_INIT; }
      break;

      case SERVER_ARGS_INIT :                               // init next args label
      if ( _buffer[i] == ' ') { _buffer[i] = 0; mode = HTTP_BAD_REQUEST; break; }
      _args[ _argsCount++  ].label = _buffer + i;
      mode = SERVER_ARGS_READ;                              // no break = include current char in args read

      case SERVER_ARGS_READ :                               // read nest args label
      if ( _buffer[i] == '=') { _buffer[i] = 0; mode = SERVER_ARGS_NEXT; }
      if ( _buffer[i] == '&') { _buffer[i] = 0; mode = SERVER_ARGS_INIT; }
      if ( _buffer[i] == ' ') { _buffer[i] = 0; mode = SERVER_HTTP_INIT; }
      break;

      case SERVER_ARGS_NEXT :                               // read next args value
      _args[ _argsCount - 1].value = _buffer + i;
      mode = SERVER_ARGS_READ;
      break;

      case SERVER_HTTP_INIT :                               // init HTTP version
      http = _buffer + i;
      mode = SERVER_HTTP_READ;                              // no break = include current char in version read

      case SERVER_HTTP_READ :                               // read HTTP version
      if ( _buffer[i] == '\r') { _buffer[i] = 0; }
      break;

      case HTTP_BAD_REQUEST :                               //
      break;
    }

    mode = (( _pathCount < MAX_PATHCOUNT) &&
            ( _argsCount < MAX_ARGSCOUNT)) ? mode : HTTP_BAD_REQUEST;
  }

  _header = true;
  _method = HTTP_ANY;                                       // determine method index
  if ( strCmp( meth, "GET"    )) { _method = HTTP_GET;     } else
  if ( strCmp( meth, "POST"   )) { _method = HTTP_POST;    } else
  if ( strCmp( meth, "DELETE" )) { _method = HTTP_DELETE;  } else
  if ( strCmp( meth, "OPTIONS")) { _method = HTTP_OPTIONS; } else
  if ( strCmp( meth, "PUT"    )) { _method = HTTP_PUT;     } else
  if ( strCmp( meth, "PATCH"  )) { _method = HTTP_PATCH;   }
  _version = http + 5;                                      // strClr( vers_s + 5);

  #ifdef SIMPLEWEBSERVER_DEBUG
  VALUE( _method); VALUE( _version) LF;

  VALUE( _pathCount);
  for ( int i = 0; i < _pathCount; i++) {
    VALUE( _path[i]);
  } LF;

  VALUE( _argsCount);
  for ( int i = 0; i < _argsCount; i++) {
    VALUE( _args[i].label); VALUE( _args[i].value);
  } LF;
  #endif

  return ( mode != HTTP_BAD_REQUEST);
}

#ifdef SIMPLEWEBSERVER_DEBUG
#define CPRINT(S) _client.print(S); PRINT(S);
#else
#define CPRINT(S) _client.print(S);
#endif

// send response header to client (code, content size, content type)
void SimpleWebServer::_sendHeader( int code, size_t size, const char* content_type)
{
  if ( _client.connected() && _header) {                    // if client session still active
    CPRINT( F( "HTTP/1.1 ")); CPRINT( code); CPRINT( " ");  // send return code
    CPRINT( HTTP_CodeMessage( code));
    CPRINT( "\r\n");

  //CPRINT( "Host"); CPRINT( Ethernet.getHostName();

    CPRINT( F( "User-Agent: "  )); CPRINT( F( "Arduino-ethernet"));
    CPRINT( "\r\n");                                        // send additional headers

    CPRINT( F( "Content-Length: ")); CPRINT( size);
    CPRINT( "\r\n");

    CPRINT( F( "Content-Type: "  )); CPRINT( content_type ? content_type : "text/html");
    CPRINT( "\r\n");

    CPRINT( F( "Connection: "    )); CPRINT( F( "close"));
    CPRINT( "\r\n");
    CPRINT( "\r\n");

    _header = false;
  }
}

// send content to client (content)
void SimpleWebServer::_sendContent( const char* content)
{
  if ( _client.connected()) {                               // if client session still active
    CPRINT( content);                                       // send content
    CPRINT( "\r\n");
  }
}

// send content to client (FLASH content)
void SimpleWebServer::_sendContent( const __FlashStringHelper* content)
{
  if ( _client.connected()) {                               // if client session still active
    CPRINT( content);                                       // send content
    CPRINT( "\r\n");
  }
}
