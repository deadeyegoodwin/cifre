// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cstdint>
#include <string>

namespace cifre {

//
// MatrixSize
//
// Represents the size of a matrix (2 dimensional vector), in rows and
// columns.
//
class MatrixSize {
public:
  // Zero-sized matrix.
  MatrixSize() : MatrixSize(0, 0) {}

  // A square matrix with 'cnt' rows and columns.
  MatrixSize(size_t cnt) : MatrixSize(cnt, cnt) {}

  // A matrix with 'rcnt' rows and 'ccnt' columns.
  MatrixSize(size_t rcnt, size_t ccnt) : row_cnt_(rcnt), column_cnt_(ccnt) {}

  // Create a MatrixSize by parsing a string with the format
  // "<rows>x<columns>", for example "4x3"; or format
  // "<rows>,<columns>", for example "4,3".
  MatrixSize(const std::string& str);

  // Operators
  bool operator==(const MatrixSize& other) const {
    return (row_cnt_ == other.row_cnt_) && (column_cnt_ == other.column_cnt_); }
  bool operator!=(const MatrixSize& other) const {
    return !(*this == other); }

  // Return the number of rows in the matrix.
  size_t RowCount() const { return row_cnt_; }

  // Return the number of columns in the matrix.
  size_t ColumnCount() const { return column_cnt_; }

  // Return the number of elements in the matrix (rows * cols).
  size_t ElementCount() const { return row_cnt_ * column_cnt_; }

  // Return the matrix size as a std::pair { row-count, column-count }.
  std::pair<size_t, size_t> ToPair() const { return { row_cnt_, column_cnt_ }; }

  // Return string representation of the matrix size.
  std::string ToString() const;

private:
  size_t row_cnt_;
  size_t column_cnt_;
};

//
// MatrixSizeHash
//
// Functor for hashing MatrixSize.
//
struct MatrixSizeHash {
  size_t operator()(const MatrixSize* a) const { return operator()(*a); }
  size_t operator()(const MatrixSize& a) const {
    return (a.RowCount() << 37) ^ a.ColumnCount(); }
};

//
// MatrixSizeEqual
//
// Functor for comparing MatrixSize.
//
struct MatrixSizeEqual {
  bool operator()(const MatrixSize* a, const MatrixSize* b) const {
    return (*a) == (*b); }
  bool operator()(const MatrixSize& a, const MatrixSize& b) const {
    return a == b; }
};

//
// MatrixCoordinate
//
// Represents a position of a matrix element as a row and
// column. Negative coordinate values are allowed to support using
// this class for deltas, etc.
//
class MatrixCoordinate {
public:
  // Zero-coordinate.
  MatrixCoordinate() : MatrixCoordinate(0, 0) {}

  // A matrix coordinate.
  MatrixCoordinate(int64_t r, int64_t c) : row_(r), column_(c) {}

  // Create a MatrixCoordinate by parsing a string with the format
  // "<row>,<column>", for example "4,3".
  MatrixCoordinate(const std::string& str);

  // Operators
  bool operator==(const MatrixCoordinate& other) const {
    return (row_ == other.row_) && (column_ == other.column_); }
  bool operator!=(const MatrixCoordinate& other) const {
    return !(*this == other); }

  // Return the row.
  int64_t Row() const { return row_; }

  // Return the column.
  int64_t Column() const { return column_; }

  // Return the matrix coordinate as a std::pair { row, column }.
  std::pair<int64_t, int64_t> ToPair() const { return { row_, column_ }; }

  // Return string representation of the matrix coordinate.
  std::string ToString() const;

private:
  int64_t row_;
  int64_t column_;
};

//
// MatrixCoordinateHash
//
// Functor for hashing MatrixCoordinate.
//
struct MatrixCoordinateHash {
  size_t operator()(const MatrixCoordinate* a) const { return operator()(*a); }
  size_t operator()(const MatrixCoordinate& a) const {
    return (a.Row() << 37) ^ a.Column(); }
};

//
// MatrixCoordinateEqual
//
// Functor for comparing MatrixCoordinate.
//
struct MatrixCoordinateEqual {
  bool operator()(const MatrixCoordinate* a, const MatrixCoordinate* b) const {
    return (*a) == (*b); }
  bool operator()(const MatrixCoordinate& a, const MatrixCoordinate& b) const {
    return a == b; }
};

} // namespace cifre
