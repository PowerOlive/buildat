// http://www.apache.org/licenses/LICENSE-2.0
// Copyright 2014 Perttu Ahola <celeron55@gmail.com>
#include "json.h"
#include <vector>
#include <map>
#include <exception>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <streambuf>
#include <cinttypes> // PRId64
#include <cmath> // isnan/isinf
#include <cfloat> // DBL_MAX/DBL_MIN
#include "sajson.h"

using namespace json;

inline std::string serializeJsonString(const std::string &plain)
{
	std::string out;
	size_t i = 0;
	for(; i < plain.size(); i++){
		const char &c = plain[i];
		if(c < 32 || c > 126 || c == '"' || c == '\\')
			goto unclean;
	}
	out.reserve(plain.size() + 2);
	out += "\"";
	out += plain;
	out += "\"";
	return out;
unclean:
	std::ostringstream os(std::ios::binary);
	os<<"\"";
	for(size_t i = 0; i < plain.size(); i++)
	{
		unsigned char c = plain[i];
		switch(c)
		{
		case '"':
			os<<"\\\"";
			break;
		case '\\':
			os<<"\\\\";
			break;
		case '/':
			os<<"\\/";
			break;
		case '\b':
			os<<"\\b";
			break;
		case '\f':
			os<<"\\f";
			break;
		case '\n':
			os<<"\\n";
			break;
		case '\r':
			os<<"\\r";
			break;
		case '\t':
			os<<"\\t";
			break;
		default:
			{
				// NOTE: This isn't correct utf-8 parsing but it
				// generally works as a counterpart to json::load_string()
				if(c >= 32)
				{
					os<<c;
				}
				else
				{
					unsigned long cnum = (unsigned long) (unsigned char) c;
					os<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<cnum;
				}
				break;
			}
		}
	}
	os<<"\"";
	return os.str();
}

namespace json
{
struct ValuePrivate
{
	std::string s;
	std::vector<Value> a;
	std::map<std::string, Value> o;
};
}

/* Value */

json::Value::Value(const Value &other): p(NULL), type(T_UNDEFINED){
	*this = other;
}
json::Value::Value(): p(NULL), type(T_UNDEFINED){
}
json::Value::Value(bool b): p(NULL), type(T_BOOL){
	value.b = b;
}
json::Value::Value(int64_t i): p(NULL), type(T_INT){
	value.i = i;
}
json::Value::Value(int i): p(NULL), type(T_INT){
	value.i = i;
}
json::Value::Value(double f): p(NULL), type(T_FLOAT){
	value.f = f;
}
json::Value::Value(float f): p(NULL), type(T_FLOAT){
	value.f = f;
}
json::Value::Value(const std::string &s): p(NULL), type(T_STRING){
	p = new ValuePrivate();
	p->s = s;
}
json::Value::Value(const char *s): p(NULL), type(T_STRING){
	p = new ValuePrivate();
	p->s = s;
}

json::Value::~Value(){
	delete p;
	p = NULL;
}

Value &json::Value::operator=(const Value &other)
{
	if(this == &other) return *this;
	type = other.type;
	switch(type){
	case T_UNDEFINED:
		delete p;
		p = NULL;
		break;
	case T_NULL:
		delete p;
		p = NULL;
		break;
	case T_BOOL:
		delete p;
		p = NULL;
		value.b = other.value.b;
		break;
	case T_INT:
		delete p;
		p = NULL;
		value.i = other.value.i;
		break;
	case T_FLOAT:
		delete p;
		p = NULL;
		value.f = other.value.f;
		break;
	case T_STRING:
		if(!p) p = new ValuePrivate();
		else  *p = ValuePrivate();
		p->s = other.p->s;
		break;
	case T_ARRAY:
		if(!p) p = new ValuePrivate();
		else  *p = ValuePrivate();
		p->a = other.p->a;
		break;
	case T_OBJECT:
		if(!p) p = new ValuePrivate();
		else  *p = ValuePrivate();
		p->o = other.p->o;
		break;
	}
	return *this;
}

bool json::Value::is_undefined() const {
	return type == T_UNDEFINED;
}
bool json::Value::is_object() const {
	return type == T_OBJECT;
}
bool json::Value::is_array() const {
	return type == T_ARRAY;
}
bool json::Value::is_string() const {
	return type == T_STRING;
}
bool json::Value::is_integer() const {
	return type == T_INT;
}
bool json::Value::is_real() const {
	return type == T_FLOAT;
}
bool json::Value::is_number() const {
	return type == T_INT || type == T_FLOAT;
}
bool json::Value::is_true() const {
	return type == T_BOOL && value.b == true;
}
bool json::Value::is_false() const {
	return type == T_BOOL && value.b == false;
}
bool json::Value::is_boolean() const {
	return type == T_BOOL;
}
bool json::Value::is_null() const {
	return type == T_NULL;
}

unsigned int json::Value::size() const {
	switch(type){
	default:
		return 0;
	case T_ARRAY:
		return p->a.size();
	case T_OBJECT:
		return p->o.size();
	}
}

const Value &json::Value::at(int index) const {
	static const Value dummy;
	if(index < 0)
		return dummy;
	else
		return at((unsigned int)index);
}
const Value &json::Value::at(unsigned int index) const {
	static const Value dummy;
	switch(type){
	default:
		return dummy;
	case T_ARRAY:
		if(index >= p->a.size())
			return dummy;
		return p->a[index];
	}
}

Value &json::Value::operator[](int index){
	return (*this)[(unsigned int)index];
}
Value &json::Value::operator[](unsigned int index){
	switch(type){
	default:
		throw MutableDoesNotExist();
	case T_ARRAY:
		if(index >= p->a.size())
			throw MutableDoesNotExist();
		return p->a[index];
	}
}

const Value &json::Value::get(const char *key) const {
	return get(std::string(key));
}
const Value &json::Value::get(const std::string &key) const {
	static const Value dummy;
	switch(type){
	case T_OBJECT: {
			auto it = p->o.find(key);
			if(it == p->o.end())
				return dummy;
			return it->second;
		}
		break;
	default:
		return dummy;
	}
}

Value &json::Value::operator[](const char *key){
	return (*this)[std::string(key)];
}
Value &json::Value::operator[](const std::string &key){
	switch(type){
	case T_OBJECT: {
			auto it = p->o.find(key);
			if(it == p->o.end())
				throw MutableDoesNotExist();
			return it->second;
		}
		break;
	default:
		throw MutableDoesNotExist();
	}
}

void json::Value::clear(){
	switch(type){
	default:
		break;
	case T_ARRAY:
		p->a.clear();
	case T_OBJECT:
		p->o.clear();
	}
}

const char *json::Value::as_cstring() const {
	switch(type){
	default:
		return NULL;
	case T_STRING:
		return p->s.c_str();
	}
}
std::string json::Value::as_string(const std::string &default_value) const {
	switch(type){
	default:
		return default_value;
	case T_STRING:
		return p->s;
	}
}
int json::Value::as_integer(int default_value) const {
	switch(type){
	default:
		return default_value;
	case T_INT:
		return value.i;
	}
}
double json::Value::as_real(double default_value) const {
	switch(type){
	default:
		return default_value;
	case T_FLOAT:
		return value.f;
	}
}
double json::Value::as_number(double default_value) const {
	switch(type){
	default:
		return default_value;
	case T_INT:
		return value.i;
	case T_FLOAT:
		return value.f;
	}
}
bool json::Value::as_boolean(bool default_value) const {
	switch(type){
	default:
		return default_value;
	case T_BOOL:
		return value.b;
	}
}

Value& json::Value::set_key(const char *key, const Value &value){
	if(value.type == T_UNDEFINED)
		return *this;
	if(type == T_OBJECT)
		p->o[key] = value;
	return *this;
}

Value& json::Value::set_key(const std::string &key, const Value &value){
	if(value.type == T_UNDEFINED)
		return *this;
	if(type == T_OBJECT)
		p->o[key] = value;
	return *this;
}

Value& json::Value::set(const char *key, const Value &value){
	return set_key(key, value);
}

Value& json::Value::set(const std::string &key, const Value &value){
	return set_key(key, value);
}

Value& json::Value::set_at(unsigned int index, const Value &value){
	if(value.type == T_UNDEFINED)
		return *this;
	if(type == T_ARRAY){
		if(p->a.size() < index + 1)
			p->a.resize(index + 1);
		p->a[index] = value;
	}
	return *this;
}

Value& json::Value::append(const Value &value){
	if(value.type == T_UNDEFINED)
		return *this;
	if(type == T_ARRAY)
		p->a.push_back(value);
	return *this;
}

Value& json::Value::del_key(const char *key){
	return del_key(std::string(key));
}

Value& json::Value::del_key(const std::string &key){
	if(type == T_OBJECT)
		p->o.erase(key);
	return *this;
}

Value& json::Value::del_at(unsigned int index){
	if(type == T_ARRAY){
		if(p->a.size() > index)
			p->a[index] = Value();
	}
	return *this;
}

Value& json::Value::insert_at(unsigned int index, const Value &value){
	if(value.type == T_UNDEFINED)
		return *this;
	if(type == T_ARRAY){
		if(p->a.size() < index + 1)
			p->a.resize(index + 1);
		p->a[index] = value;
	}
	return *this;
}

int json::Value::save_file(const char *path, int flags) const {
	std::string s = stringify(flags);
	std::ofstream t(path, std::ios::binary);
	if(!t.good())
		return -1;
	t<<s;
	if(!t.good())
		return -1;
	return 0;
}

std::string json::Value::stringify(int flags) const {
	switch(type){
	case T_UNDEFINED:
		return "undefined";
	case T_NULL:
		return "null";
	case T_BOOL:
		return value.b ? "true" : "false";
	case T_INT: {
			std::ostringstream os(std::ios::binary);
			os<<value.i;
			return os.str();
		}
	case T_FLOAT: {
			// Silently convert NaN and infinity to valid JSON numbers
			double f = value.f;
			if(isnan(f))
				f = 0.0;
			else if(isinf(f))
				f = (f >= 0) ?  DBL_MAX : DBL_MIN;
			// Convert to string
			std::ostringstream os(std::ios::binary);
			os<<value.f;
			return os.str();
		}
	case T_STRING:
		return serializeJsonString(p->s);
	case T_ARRAY: {
			std::ostringstream os(std::ios::binary);
			os<<"[";
			bool first = true;
			for(const auto &v : p->a){
				if(!first) os<<",";
				first = false;
				os<<v.stringify();
			}
			os<<"]";
			return os.str();
		}
	case T_OBJECT: {
			std::ostringstream os(std::ios::binary);
			os<<"{";
			bool first = true;
			for(const auto &it : p->o){
				if(!first) os<<",";
				first = false;
				os<<serializeJsonString(it.first)<<":"<<it.second.stringify();
			}
			os<<"}";
			return os.str();
		}
	}
	return "<invalid value>";
}

bool json::Value::operator==(const Value &value) const {
	return (stringify() == value.stringify());
}

const Value json::Value::deepcopy() const {
	return *this;
}

/* Iterator */

namespace json
{
struct IteratorPrivate {
	std::vector<Value>::const_iterator a;
	std::map<std::string, Value>::const_iterator o;
};
}

json::Iterator::Iterator(const Value &v):
	v(v), p(new IteratorPrivate())
{
	switch(v.type){
	case Value::T_ARRAY:
		p->a = v.p->a.begin();
		break;
	case Value::T_OBJECT:
		p->o = v.p->o.begin();
		break;
	default:
		break;
	}
}

json::Iterator::~Iterator()
{
	delete p;
}

// increment iterator
void json::Iterator::next(){
	switch(v.type){
	case Value::T_ARRAY:
		if(p->a != v.p->a.end())
			++p->a;
		break;
	case Value::T_OBJECT:
		if(p->o != v.p->o.end())
			++p->o;
		break;
	default:
		break;
	}
}

Iterator &json::Iterator::operator++(){
	next();
	return *this;
}

// test if iterator is still valid
bool json::Iterator::valid() const {
	switch(v.type){
	case Value::T_ARRAY:
		return p->a != v.p->a.end();
	case Value::T_OBJECT:
		return p->o != v.p->o.end();
	default:
		return false;
	}
}

json::Iterator::operator bool() const {
	return valid();
}

const char *json::Iterator::ckey() const {
	switch(v.type){
	case Value::T_OBJECT:
		if(p->o == v.p->o.end())
			return NULL;
		return p->o->first.c_str();
	default:
		return NULL;
	}
}

std::string json::Iterator::key() const {
	switch(v.type){
	case Value::T_OBJECT:
		if(p->o == v.p->o.end())
			return std::string();
		return p->o->first;
	default:
		return std::string();
	}
}

const Value &json::Iterator::value() const {
	static const Value dummy;
	switch(v.type){
	case Value::T_ARRAY:
		return *p->a;
	case Value::T_OBJECT:
		return p->o->second;
	default:
		return dummy;
	}
}

const Value &json::Iterator::operator*() const {
	return value();
}

/* json */

const Value json::object(){
	Value v;
	v.type = Value::T_OBJECT;
	v.p = new ValuePrivate();
	return v;
}
const Value json::array(){
	Value v;
	v.type = Value::T_ARRAY;
	v.p = new ValuePrivate();
	return v;
}
const Value json::null(){
	Value v;
	v.type = Value::T_NULL;
	return v;
}

const Value json::load_sajson(const sajson::value &src)
{
	switch(src.get_type()){
	case sajson::TYPE_INTEGER:
		return Value(src.get_integer_value());
	case sajson::TYPE_DOUBLE:
		return Value(src.get_double_value());
	case sajson::TYPE_NULL:
		return null();
	case sajson::TYPE_FALSE:
		return Value(false);
	case sajson::TYPE_TRUE:
		return Value(true);
	case sajson::TYPE_STRING:
		return Value(src.as_string());
	case sajson::TYPE_ARRAY: {
			Value dst = array();
			dst.p->a.resize(src.get_length());
			for(size_t i = 0; i < src.get_length(); i++)
				dst.p->a[i] = load_sajson(src.get_array_element(i));
			return dst;
		}
	case sajson::TYPE_OBJECT: {
			Value dst = object();
			for(size_t i = 0; i < src.get_length(); i++){
				std::string skey = src.get_object_key(i).as_string();
				dst.p->o[skey] = load_sajson(src.get_object_value(i));
			}
			return dst;
		}
	}
	return Value();
}

const Value json::load_string(const char *string, json_error_t *error){
	//printf("load_string(%s)\n", string);
	if(error){
		error->line = -1;
		error->column = -1;
		error->text[0] = '\0';
	}
	const sajson::document doc =
			sajson::parse(sajson::string(string, strlen(string)));
	if(!doc.is_valid()){
		if(error){
			error->line = doc.get_error_line();
			error->column = doc.get_error_column();
			snprintf(error->text, sizeof(error->text), "%s",
					doc.get_error_message().c_str());
		}
		return Value();
	}
	sajson::value root = doc.get_root();
	return load_sajson(root);
}

const Value json::load_file(const char *path, json_error_t *error){
	if(error){
		error->line = -1;
		error->column = -1;
		error->text[0] = '\0';
	}
	std::ifstream t(path);
	if(!t.good()){
		snprintf(error->text, sizeof(error->text), "Failed to open file [%s]",
				path);
		return Value();
	}
	t.seekg(0, std::ios::end);
	size_t size = t.tellg();
	std::string buffer(size, ' ');
	t.seekg(0);
	t.read(&buffer[0], size);
	return json::load_string(buffer.c_str(), error);
}
// vim: set noet ts=4 sw=4:
