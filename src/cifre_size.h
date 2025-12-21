// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cstdint>
#include <string>

namespace cifre {

//
// Size2D
//
// Represents a 2D size, in rows and columns.
//
class Size2D {
public:
  // A square board size with 'cnt' rows and columns.
  Size2D(size_t cnt) : Size2D(cnt, cnt) {}

  // A rectangular board size with 'rcnt' rows and 'ccnt' columns.
  Size2D(size_t rcnt, size_t ccnt) : row_cnt_(rcnt), column_cnt_(ccnt) {}

  // Create a Size2D by parsing a string with the format
  // "<rows>x<columns>", for example "4x3"; or format
  // "<rows>,<columns>", for example "4,3".
  Size2D(const std::string& str);

  // Operators
  bool operator==(const Size2D& other) const {
    return (row_cnt_ == other.row_cnt_) && (column_cnt_ == other.column_cnt_); }
  bool operator!=(const Size2D& other) const {
    return !(*this == other); }

  // Return the number of rows in the board size.
  size_t RowCount() const { return row_cnt_; }

  // Return the number of columns in the board size.
  size_t ColumnCount() const { return column_cnt_; }

  // Return the number of cells (rows * cols) in the board size.
  size_t CellCount() const { return row_cnt_ * column_cnt_; }

  // Return the board size as a std::pair { row-count, column-count }.
  std::pair<size_t, size_t> Size() const { return { row_cnt_, column_cnt_ }; }

  // Return string representation of the board size.
  std::string ToString() const;

private:
  size_t row_cnt_;
  size_t column_cnt_;
};

} // namespace cifre
