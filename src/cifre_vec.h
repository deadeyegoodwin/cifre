// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cstdint>
#include <string>

namespace cifre {

//
// Vector2
//
// Represents a 2 dimensional vector (matrix).
//
class Vector2 {
public:
  // A square matrix with 'cnt' rows and columns.
  Vector2(size_t cnt) : Vector2(cnt, cnt) {}

  // A matrix with 'rcnt' rows and 'ccnt' columns.
  Vector2(size_t rcnt, size_t ccnt) : row_cnt_(rcnt), column_cnt_(ccnt) {}

  // Create a Vector2 by parsing a string with the format
  // "<rows>x<columns>", for example "4x3"; or format
  // "<rows>,<columns>", for example "4,3".
  Vector2(const std::string& str);

  // Operators
  bool operator==(const Vector2& other) const {
    return (row_cnt_ == other.row_cnt_) && (column_cnt_ == other.column_cnt_); }
  bool operator!=(const Vector2& other) const {
    return !(*this == other); }

  // Return the number of rows in the matrix.
  size_t RowCount() const { return row_cnt_; }

  // Return the number of columns in the matrix.
  size_t ColumnCount() const { return column_cnt_; }

  // Return the number of elements in the matrix (rows * cols).
  size_t ElementCount() const { return row_cnt_ * column_cnt_; }

  // Return the matrix size as a std::pair { row-count, column-count }.
  std::pair<size_t, size_t> ToPair() const { return { row_cnt_, column_cnt_ }; }

  // Return string representation of the matrix.
  std::string ToString() const;

private:
  size_t row_cnt_;
  size_t column_cnt_;
};

} // namespace cifre
