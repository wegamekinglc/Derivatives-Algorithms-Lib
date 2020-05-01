//
// Created by wegamekinglc on 2020/5/1.
//

#pragma once
#include <map>
#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <dal/storage/storable.hpp>
#include <dal/math/matrixs.hpp>


namespace Dal {
    namespace Archive {
        namespace Utils {
            void SetStorable(Archive::Store_& dst,
                             const String_& name,
                             const Storable_& value);
        }

        class Store_: noncopyable {
            virtual bool StoreRef(const Storable_* object) = 0;
            friend void Utils::SetStorable(Archive::Store_ &, const String_&, const Storable_&);

        public:
            virtual ~Store_() = default;
            virtual void SetType(const String_& type) = 0;
            virtual void Done() {} // states explicitly when an object is done (to enable streaming)
            virtual Store_& Child(const String_& name) = 0;
            Store_& Element(int index); // just converts to String_ name and gets a child

            virtual void operator=(double  val) = 0;
            virtual void operator=(const String_& val) = 0;
            virtual void operator=(const Date_& val) = 0;
            virtual void operator=(const Vector_<>& val) = 0;
            virtual void operator=(const Vector_<int>& val) = 0;
            virtual void operator=(const Vector_<bool>& val) = 0;
            virtual void operator=(const Vector_<String_>& val) = 0;
            virtual void operator=(const Vector_<Date_>& val) = 0;
            virtual void operator=(const Vector_<DateTime_>& val) = 0;
            virtual void operator=(const Matrix_<>& val) = 0;
            virtual void operator=(const Matrix_<String_>& val) = 0;
            virtual void operator=(const Dictionary_& val) = 0;
        };

        class Built_ {
        public:
            std::map<String_, Handle_<Storable_>> known_;
        };

        class View_;

        Handle_<Storable_> Extract(const View_& src, Built_& build);

        class View_: noncopyable {
            virtual Handle_<Storable_>& Known(Archive::Built_& built) const = 0; //returns a reference within 'built'
            friend Handle_<Storable_> Archive::Extract(const View_&, Built_&);

        public:
            virtual ~View_() = default;

            // query fundamental types
            virtual double AsDouble() = 0;
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

            // query composite types
            virtual String_ Type() const = 0;
            virtual const View_& Child(const String_& name) const = 0;
            virtual bool HasChild(const String_& name) const = 0;
            const View_& Element(int index) const;
            bool HasElement(int index) const;

            virtual void Unexpected(const String_& child_name) const = 0;
        };

        class Reader_: noncopyable {
        public:
            virtual ~Reader_() = default;
            virtual Storable_* Build() const = 0;
            virtual Storable_* Build(const View_& view, Archive::Built_& share) const =0;
        };
    }
}