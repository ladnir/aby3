#pragma once

#include <cryptoTools/Common/MatrixView.h>
#include "aby3/Engines/sh3/Sh3BinaryEvaluator.h"
#include "aby3/Engines/sh3/Sh3Encryptor.h"
#include "aby3/Engines/sh3/Sh3Evaluator.h"

namespace osuCrypto
{
    enum class TypeID
    {
        IntID = 0,
        StringID = 1        
    };

    struct DataType
    {
        virtual TypeID getTypeID() const = 0;
        virtual u64 getBitCount() const = 0;
    };

    struct IntType : public DataType
    {
        u64 mBitCount = 0;

        IntType(u64 bc) : mBitCount(bc) {}

        u64 getBitCount() const override { return mBitCount; }
        TypeID getTypeID() const override { return TypeID::IntID; }
    };

    struct StringType : public DataType
    {
        u64 mCharCount = 0;
        StringType(u64 bc) : mCharCount(bc / 8) { if (bc % 8) throw RTE_LOC; }

        u64 getBitCount() const override { return mCharCount * 8; }
        TypeID getTypeID() const override { return TypeID::StringID; }

    };

    struct ColumnBase 
    {
        std::shared_ptr<const DataType> mType;
        std::string mName;

        ColumnBase() = default;
        ColumnBase(const ColumnBase&) = default;
        ColumnBase(ColumnBase&&) = default;

        ColumnBase(std::string name, TypeID type, u64 size)
        {
            mName = std::move(name);
            if (type == TypeID::IntID)
                mType = std::make_shared<IntType>(size);
            else if (type == TypeID::StringID)
                mType = std::make_shared<StringType>(size);
            else
                throw RTE_LOC;
        }

        u64 getBitCount() const { return mType->getBitCount(); }
        TypeID getTypeID() const { return mType->getTypeID(); }
    };

    struct Column : public ColumnBase , public aby3::Sh3::i64Matrix
    {

        Column() = delete;
        Column(const Column&) = default;
        Column(Column&&) = default;

        Column(std::string name, TypeID type, u64 size)
            : ColumnBase(std::move(name), type, size)
        { }
    };

    struct SharedColumn : public ColumnBase, public aby3::Sh3::sbMatrix
    {
        SharedColumn() = default;
        SharedColumn(const SharedColumn&) = default;
        SharedColumn(SharedColumn&&) = default;

        SharedColumn(std::string name, TypeID type, u64 size)
            : ColumnBase(std::move(name), type, size)
        { }
    };

    using ColumnInfo = std::tuple<std::string, TypeID, u64>;

    class Table
    {
    public:
        std::vector<Column> mColumns;

        Table() = default;
        Table(const Table&) = default;
        Table(Table&&) = default;
        Table(u64 rows, std::vector<ColumnInfo> columns)
        {
            init(rows, columns);
        }

        void init(u64 rows, std::vector<ColumnInfo> columns)
        {
            mColumns.reserve(columns.size());
            for (u64 i = 0; i < columns.size(); ++i)
            {
                mColumns.emplace_back(
                    std::get<0>(columns[i]),
                    std::get<1>(columns[i]),
                    std::get<2>(columns[i])
                );
                auto size = (std::get<2>(columns[i]) + 63) / 64;
                mColumns.back().resize(rows, size);
            }
        }

        u64 rows() { return mColumns.size() ? mColumns[0].rows() : 0; }




    };

    class SharedTable
    {
    public:
        // shared keys are stored in packed binary format. i.e. XOR shared and trasposed.
        std::vector<SharedColumn> mColumns;

        struct ColRef
        {
            SharedTable& mTable;
            SharedColumn& mCol;

            ColRef(SharedTable& t, SharedColumn& c)
                : mTable(t), mCol(c)
            {}

            ColRef(const ColRef&) = default;
            ColRef(ColRef&&) = default;

        };


        ColRef operator[](std::string c)
        {
            for (u64 i = 0; i < mColumns.size(); ++i)
            {
                if (mColumns[i].mName == c)
                    return { *this, mColumns[i] };
            }

            throw RTE_LOC;
        }


        u64 rows();
    };

}