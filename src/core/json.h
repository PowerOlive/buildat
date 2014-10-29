// http://www.apache.org/licenses/LICENSE-2.0
// Copyright 2014 Perttu Ahola <celeron55@gmail.com>
#pragma once
#include <stdint.h>
#include <string>
#include <exception>

namespace sajson {
	class value;
}

namespace json
{
	class MutableDoesNotExist: public std::exception {
		virtual const char* what() const throw()
		{
			return "MutableDoesNotExist";
		}
	};

	struct ValuePrivate;
	class Value
	{
	public:
		enum Type {
			T_UNDEFINED,
			T_NULL,
			T_BOOL,
			T_INT,
			T_FLOAT,
			T_STRING,
			T_ARRAY,
			T_OBJECT,
			//T_RAW,
		};
	private:
		friend const Value json::object();
		friend const Value json::array();
		friend const Value json::null();
		friend const Value json::load_sajson(const sajson::value &src);
		friend class Iterator;

		mutable ValuePrivate *p;
		Type type;
		union {
			bool b;
			int64_t i;
			double f;
		} value;
	public:
		Value(const Value &other);
		Value();
		Value(bool b);
		Value(int64_t i);
		Value(int i);
		Value(double f);
		Value(float f);
		Value(const std::string &s);
		Value(const char *s);

		~Value();

		// assignment operator
		Value& operator=(const Value &value);

		// get type
		Type get_type() const {return type;}
		// describe type
		const char* desc_type() const;

		// check value type
		bool is_undefined() const;
		bool is_object() const;
		bool is_array() const;
		bool is_string() const;
		bool is_integer() const;
		bool is_real() const;
		bool is_number() const;
		bool is_true() const;
		bool is_false() const;
		bool is_boolean() const;
		bool is_null() const;

		// get size of array or object
		unsigned int size() const;

		// get value at array index
		const Value& at(int index) const;
		const Value& at(unsigned int index) const;

		Value& operator[](int index);
		Value& operator[](unsigned int index);

		// get object property
		const Value& get(const char *key) const;
		const Value& get(const std::string &key) const;

		Value& operator[](const char *key);
		Value& operator[](const std::string &key);

		// clear all array/object values
		void clear();

		// get value cast to specified type
		const char* as_cstring() const;
		const std::string& as_string() const;
		const int64_t& as_integer() const;
		const double& as_real() const;
		const double& as_number() const;
		const bool& as_boolean() const;

		// get value according to template type
		template<typename T> const T& as() const;

		// set an object property (converts value to object is not one already)
		Value& set_key(const char *key, const Value &value);
		Value& set_key(const std::string &key, const Value &value);
		Value& set(const char *key, const Value &value);
		Value& set(const std::string &key, const Value &value);

		// set an array index
		Value& set_at(unsigned int index, const Value &value);

		Value& append(const Value &value);

		// delete an object key
		Value& del_key(const char *key);

		Value& del_key(const std::string &key);

		// delete an item from an array by index
		Value& del_at(unsigned int index);

		// insert an item into an array at a given index
		Value& insert_at(unsigned int index, const Value &value);

		// write the value to a file
		int save_file(const char *path, int flags = 0) const;

		// serialize stuff like JSON.stringify()
		std::string stringify(int flags = 0) const;

		// serialize stuff using stringfy() and then compare
		bool operator==(const Value &value) const;

		const Value deepcopy() const;
	};

	template<> inline const json::Value& Value::as<json::Value>() const {
		return *this;
	}
	template<> inline const std::string& Value::as<std::string>() const {
		return as_string();
	}
	template<> inline const int64_t& Value::as<int64_t>() const {
		return as_integer();
	}
	template<> inline const  double& Value::as<double> () const {
		return as_number();
	}
	template<> inline const    bool& Value::as<bool>   () const {
		return as_boolean();
	}

	// iterators over a JSON object
	struct IteratorPrivate;
	class Iterator {
		const Value &v;
		IteratorPrivate *p;
	public:
		// construct a new iterator for a given object
		Iterator(const Value &value);

		~Iterator();

		// increment iterator
		void next();

		Iterator& operator++();

		// test if iterator is still valid
		bool valid() const;

		operator bool() const;

		// get key
		const char* ckey() const;

		std::string key() const;

		// get value
		const Value& value() const;

		// dereference value
		const Value& operator*() const;

	private:
		// disallow copying
		Iterator(const Iterator&);
		Iterator& operator=(const Iterator&);
	};

	// create a new empty object
	const Value object();

	// create a new empty array
	const Value array();

	// create a new null value
	const Value null();

	const Value load_sajson(const sajson::value &src);

	struct json_error_t {
		int line;
		int column;
		char text[1000];
	};

	// load a file as a JSON value
	const Value load_file(const char *path, json_error_t *error = 0);

	// load a string as a JSON value
	const Value load_string(const char *string, json_error_t *error = 0);

	template<typename T>
	struct JsonTraits
	{
	};
}

#define JSON_INDENT(x)  // Dummy for now
// codestyle:disable (muh beautiful alignments)
// vim: set noet ts=4 sw=4:
