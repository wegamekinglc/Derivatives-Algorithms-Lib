//
// Created by wegamekinglc on 2020/5/1.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/cell.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/platform/optionals.hpp>
#include <dal/storage/storable.hpp>
#include <dal/string/strings.hpp>
#include <map>

namespace Dal {
    namespace Archive {
        namespace Utils {
            void SetStorable(Archive::Store_& dst, const String_& name, const Storable_& value);
        }

        class Store_ : noncopyable {
            virtual bool StoreRef(const Storable_* object) = 0;
            friend void Utils::SetStorable(Archive::Store_&, const String_&, const Storable_&);

        public:
            virtual ~Store_() = default;
            virtual void SetType(const String_& type) = 0;
            virtual void Done() {} // states explicitly when an object is done (to enable streaming)
            virtual Store_& Child(const String_& name) = 0;
            Store_& Element(int index); // just converts to String_ name and gets a child

            virtual Store_& operator=(double val) = 0;
            virtual Store_& operator=(const String_& val) = 0;
            virtual Store_& operator=(const Date_& val) = 0;
            virtual Store_& operator=(const DateTime_& val) = 0;
            virtual Store_& operator=(const Vector_<>& val) = 0;
            virtual Store_& operator=(const Vector_<int>& val) = 0;
            virtual Store_& operator=(const Vector_<bool>& val) = 0;
            virtual Store_& operator=(const Vector_<String_>& val) = 0;
            virtual Store_& operator=(const Vector_<Date_>& val) = 0;
            virtual Store_& operator=(const Vector_<DateTime_>& val) = 0;
            virtual Store_& operator=(const Matrix_<>& val) = 0;
            virtual Store_& operator=(const Matrix_<String_>& val) = 0;
            virtual Store_& operator=(const Matrix_<Cell_>& val) = 0;
            virtual Store_& operator=(const Dictionary_& val) = 0;
        };

        class Built_ {
        public:
            std::map<String_, Handle_<Storable_>> known_;
        };

        class View_;

        Handle_<Storable_> Extract(const View_& src, Built_& build);

        class View_ : noncopyable {
            virtual Handle_<Storable_>& Known(Archive::Built_& built) const = 0; // returns a reference within 'built'
            friend Handle_<Storable_> Archive::Extract(const View_&, Built_&);

        public:
            virtual ~View_() = default;

            // query fundamental types
            virtual double AsDouble() const = 0;
            virtual int AsInt() const = 0;
            virtual bool AsBool() const = 0;
            virtual String_ AsString() const = 0;
            virtual Date_ AsDate() const = 0;
            virtual Dictionary_ AsDictionary() const = 0;
            virtual Vector_<> AsDoubleVector() const = 0;
            virtual Vector_<int> AsIntVector() const = 0;
            virtual Vector_<bool> AsBoolVector() const = 0;
            virtual Vector_<String_> AsStringVector() const = 0;
            virtual Vector_<Date_> AsDateVector() const = 0;
            virtual Vector_<DateTime_> AsDateTimeVector() const = 0;
            virtual Matrix_<> AsDoubleMatrix() const = 0;
            virtual Matrix_<String_> AsStringMatrix() const = 0;
            virtual Matrix_<Cell_> AsCellMatrix() const = 0;

            // query composite types
            virtual String_ Type() const = 0;
            virtual const View_& Child(const String_& name) const = 0;
            virtual bool HasChild(const String_& name) const = 0;
            const View_& Element(int index) const;
            bool HasElement(int index) const;

            virtual void Unexpected(const String_& child_name) const = 0;
        };

        template <class T_ = Storable_> struct Builder_ {
            Built_& share_;
            const char* name_;
            const char* type_;
            Builder_(Built_& share, const char* name, const char* type) : share_(share), name_(name), type_(type) {}
            Handle_<T_> operator()(const View_& src) const {
                NOTICE2("Child name", name_);
                Handle_<Storable_> object = Extract(src, share_);
                NOTICE2("Expected type", type_);
                Handle_<T_> ret_val = handle_cast<T_>(object);
                return ret_val;
            }
        };

        class Reader_ : noncopyable {
        public:
            virtual ~Reader_() = default;
            virtual Storable_* Build() const = 0;
            virtual Storable_* Build(const View_& view, Archive::Built_& share) const = 0;
        };

        void Register(const String_& type, const Reader_* d_type);

        namespace Utils {
            template <class T_> inline void Set(Store_& dst, const String_& name, const T_& value) {
                dst.Child(name) = value;
            }

            template <class T_> inline void Set(Store_& dst, const String_& name, const Handle_<T_>& value) {
                REQUIRE(value, "Can't serialize a null object");
                SetStorable(dst, name, dynamic_cast<const T_&>(*value));
            }

            // helpers on top of raw Set()
            template <class T_> inline void SetOptional(Store_& dst, const String_& name, const T_& value) {
                if (value != T_())
                    Set(dst, name, value);
            }

            template <class T_>
            inline void SetOptional(Store_& dst, const String_& name, const std::optional<T_>& value) {
                if (value)
                    Set(dst, name, value.get());
            }

            template <class T_> inline void SetMultiple(Store_& dst, const String_& name, const Vector_<T_>& values) {
                for (int i = 0; i < values.size(); ++i)
                    Set(dst, name + String::FromInt(i), values[i]);
            }

            template <class E_, class T_>
            inline void Get(const View_& src, const String_& name, E_* value, const T_& translator) {
                *value = translator(src.Child(name));
            }

            template <class E_, class T_>
            inline void GetOptional(const View_& src, const String_& name, E_* value, const T_& translator) {
                if (src.HasChild(name))
                    Get(src, name, value, translator);
            }

            template <class E_, class T_>
            inline void GetMultiple(const View_& src, const String_& name, Vector_<E_>* values, const T_& translator) {
                int curr_index = values->size();
                while (true) {
                    String_ childName = String_(name + String::FromInt(curr_index));
                    if (!src.HasChild(childName))
                        break;
                    values->push_back(translator(src.Child(childName)));
                    ++curr_index;
                }
            }
        } // namespace Utils
    }     // namespace Archive
} // namespace Dal