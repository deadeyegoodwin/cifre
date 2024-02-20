// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "cifre_state.h"
#include "common_error.h"
#include "common_logger.h"
#include "common_util.h"

namespace cifre {

//
// BoardSize2D
//
// Represents the size of a 2D board, in rows and columns.
//
class BoardSize2D {
public:
  // A square board size with 'cnt' rows and columns.
  BoardSize2D(size_t cnt) : BoardSize2D(cnt, cnt) {}

  // A rectangular board size with 'rcnt' rows and 'ccnt' columns.
  BoardSize2D(size_t rcnt, size_t ccnt) : row_cnt_(rcnt), column_cnt_(ccnt) {}

  // Create a BoardSize2D by parsing a string with the format
  // "<rows>x<columns>", for example, "4x3".
  BoardSize2D(const std::string& str);

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

//
// Board2D
//
// A 2D game board where the value of each board "cell" is represented
// by a single value of type T. Acts as a cifre::State with the triton
// representation being a flattened row-major ordering of the cell T
// values. Derived classes must implement appropriate action and board
// update logic.
//
template <typename T>
class Board2D : public cifre::State {
public:
  // Create a board of 'size'.
  Board2D(const BoardSize2D& size);

  // Copy constructor.
  Board2D(const Board2D& other);

  // The size of the board.
  const BoardSize2D& Size() const { return size_; }

  // Return the value of the cell at the given row and column index.
  const T& CellValueAt(size_t row_idx, size_t col_idx) const;

  // Set the value of the cell at the given row and column index.
  void SetCellValueAt(const T& value, size_t row_idx, size_t col_idx);

  // Return the value of the cell at the flattened index 'flat_idx'.
  const T& CellValueAtFlattened(size_t flat_idx) const;

  // Set the value of the cell at the flattened index 'flat_idx'.
  void SetCellValueAtFlattened(const T& value, size_t flat_idx);

  // From cifre::State, get the representation of the state suitable
  // for feeding to the model via triton. Returns flattened row-major
  // ordering of the cel values.
  const std::vector<uint8_t>& TritonRepresentation() const override;

protected:
  const BoardSize2D size_;

  // Triton-friendly cell contents. 'cells_' is simply a type-casted
  // pointer into 'cells_flattened_'.
  std::vector<uint8_t> cells_flattened_;
  T* cells_;
};

template <typename T>
Board2D<T>::Board2D(const BoardSize2D& size) :
  size_(size)
{
  // Initialize storage for flattened cell values... derived class
  // must set initial values.
  cells_flattened_.resize(size_.CellCount() * sizeof(T));
  cells_ = reinterpret_cast<T*>(cells_flattened_.data());
}

template <typename T>
Board2D<T>::Board2D(const Board2D& other) :
  size_(other.size_), cells_flattened_(other.cells_flattened_)
{
  cells_ = reinterpret_cast<T*>(cells_flattened_.data());
}

template <typename T>
const T&
Board2D<T>::CellValueAt(size_t row_idx, size_t col_idx) const
{
  THROWIF((row_idx >= size_.RowCount()) || (col_idx >= size_.ColumnCount()),
          "invalid Board2D cell coordinates");
  return cells_[(row_idx * size_.ColumnCount()) + col_idx];
}

template <typename T>
void
Board2D<T>::SetCellValueAt(const T& value, size_t row_idx, size_t col_idx)
{
  THROWIF((row_idx >= size_.RowCount()) || (col_idx >= size_.ColumnCount()),
          "invalid Board2D cell coordinates");
  cells_[(row_idx * size_.ColumnCount()) + col_idx] = value;
}

template <typename T>
const T&
Board2D<T>::CellValueAtFlattened(size_t flat_idx) const
{
  THROWIF(flat_idx >= size_.CellCount(), "invalid Board2D cell coordinates");
  return cells_[flat_idx];
}

template <typename T>
void
Board2D<T>::SetCellValueAtFlattened(const T& value, size_t flat_idx)
{
  THROWIF(flat_idx >= size_.CellCount(), "invalid Board2D cell coordinates");
  cells_[flat_idx] = value;
}

template <typename T>
const std::vector<uint8_t>&
Board2D<T>::TritonRepresentation() const
{
  return cells_flattened_;
}

} // namespace cifre
