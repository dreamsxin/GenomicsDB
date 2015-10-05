/**
 * @file   array_const_dense_cell_iterator.cc
 * @author Stavros Papadopoulos <stavrosp@csail.mit.edu>
 *
 * @section LICENSE
 *
 * The MIT License
 * 
 * @copyright Copyright (c) 2014 Stavros Papadopoulos <stavrosp@csail.mit.edu>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 * @section DESCRIPTION
 *
 * This file implements the ArrayConstDenseCellIterator class.
 */

#include "array_const_dense_cell_iterator.h"
#include "utils.h"
#include <assert.h>

/******************************************************
************* CONSTRUCTORS & DESTRUCTORS **************
******************************************************/

template<class T>
ArrayConstDenseCellIterator<T>::ArrayConstDenseCellIterator() {
  array_ = NULL;
  cell_ = NULL;
  cell_buffer_size_ = CELL_BUFFER_INITIAL_SIZE;
  cell_its_ = NULL;
  coords_match_ = false;
  current_coords_ = NULL;
  zero_cell_ = NULL;
  end_ = false;
  range_ = NULL;
  tile_its_ = NULL;
  full_overlap_ = NULL;
  is_del_ = false;
  var_size_ = false;
  cell_size_ = 0;
}

template<class T>
ArrayConstDenseCellIterator<T>::ArrayConstDenseCellIterator(
    Array* array) : array_(array) {
  // Initialize private attributes
  const ArraySchema* array_schema = array_->array_schema();
  attribute_num_ = array_schema->attribute_num();
  dim_num_ = array_schema->dim_num();
  fragment_num_ = array_->fragment_num();
  end_ = false;
  range_ = NULL;
  full_overlap_ = NULL;
  return_del_ = false;
  cell_its_ = NULL;
  cell_buffer_size_ = CELL_BUFFER_INITIAL_SIZE;
  coords_match_ = false;
  current_coords_ = new T[dim_num_];
  array_schema->get_domain_start<T>(current_coords_, range_); 
  tile_its_ = NULL;
  array_schema->init_zero_cell(zero_cell_);

  // Prepare the ids of the fragments the iterator will iterate on
  for(int i=0; i<fragment_num_; ++i)
    fragment_ids_.push_back(i);

  // Prepare the ids of the attributes the iterator will iterate on
  for(int i=0; i<=attribute_num_; ++i)
    attribute_ids_.push_back(i);

  // Case where the array is empty
  if(array_->empty()) {
    cell_size_ = 0;
    var_size_ = false;
    cell_ = NULL;
    end_ = true;
    is_del_ = false;
    return;
  }

  // Allocate space for the cell
  cell_size_ = array_schema->cell_size(attribute_ids_);
  var_size_ = (cell_size_ == VAR_SIZE);
  if(!var_size_)
    cell_ = malloc(cell_size_);
  else 
    cell_ = NULL;

  // Get first cell
  init_iterators();
  int fragment_id = get_next_cell(); 
  if(fragment_id != -1)
    advance_cell(fragment_id); 
}

template<class T>
ArrayConstDenseCellIterator<T>::ArrayConstDenseCellIterator(
    Array* array, 
    const std::vector<int>& fragment_ids,
    bool return_del) : array_(array), return_del_(return_del) {
  // Checks
  assert(fragment_ids.size() != 0);

  // Initialize private attributes
  const ArraySchema* array_schema = array_->array_schema();
  attribute_num_ = array_schema->attribute_num();
  dim_num_ = array_schema->dim_num();
  fragment_num_ = array_->fragment_num();
  end_ = false;
  range_ = NULL;
  full_overlap_ = NULL;
  cell_its_ = NULL;
  cell_buffer_size_ = CELL_BUFFER_INITIAL_SIZE;
  coords_match_ = false;
  current_coords_ = new T[dim_num_];
  array_schema->get_domain_start<T>(current_coords_, range_); 
  tile_its_ = NULL;
  array_schema->init_zero_cell(zero_cell_);

  // Prepare the ids of the fragments the iterator will iterate on
  fragment_ids_ = fragment_ids;

  // Prepare the ids of the attributes the iterator will iterate on
  for(int i=0; i<=attribute_num_; ++i)
    attribute_ids_.push_back(i);

  // Case where the array is empty
  if(array_->empty()) {
    cell_size_ = 0;
    var_size_ = false;
    cell_ = NULL;
    end_ = true;
    is_del_ = false;
    return;
  }

  // Allocate space for the cell
  cell_size_ = array_schema->cell_size(attribute_ids_);
  var_size_ = (cell_size_ == VAR_SIZE);
  if(!var_size_)
    cell_ = malloc(cell_size_);
  else 
    cell_ = NULL;

  // Get first cell
  init_iterators();
  int fragment_id = get_next_cell(); 
  if(fragment_id != -1)
    advance_cell(fragment_id); 
}

template<class T>
ArrayConstDenseCellIterator<T>::ArrayConstDenseCellIterator(
    Array* array, 
    const std::vector<int>& attribute_ids) : array_(array) {
  // Check fragment and attribute ids
  assert(array_->array_schema()->valid_attribute_ids(attribute_ids));

  // Initialize private attributes
  const ArraySchema* array_schema = array_->array_schema();
  attribute_num_ = array_schema->attribute_num();
  dim_num_ = array_schema->dim_num();
  fragment_num_ = array_->fragment_num();
  end_ = false;
  range_ = NULL;
  full_overlap_ = NULL;
  return_del_ = false;
  cell_buffer_size_ = CELL_BUFFER_INITIAL_SIZE;
  cell_its_ = NULL;
  coords_match_ = false;
  current_coords_ = new T[dim_num_];
  array_schema->get_domain_start<T>(current_coords_, range_); 
  tile_its_ = NULL;
  array_schema->init_zero_cell(zero_cell_);

  // Prepare the ids of the fragments the iterator will iterate on
  for(int i=0; i<fragment_num_; ++i)
    fragment_ids_.push_back(i);

  // Prepare the ids of the attributes the iterator will iterate on
  // The coordinates must always be part of the attributes to be
  // iterated on (to allow visitng cells of multiple fragments in order)
  attribute_ids_ = attribute_ids;
  // In case the attribute_ids is empty, include a dummy attribute to
  // gracefully handle deletions.
  if(attribute_ids_.size() == 0) 
    attribute_ids_.push_back(array_schema->smallest_attribute());
  attribute_ids_.push_back(attribute_num_);
  attribute_ids_ = rdedup(attribute_ids_);

  // Case where the array is empty
  if(array_->empty()) {
    cell_size_ = 0;
    var_size_ = false;
    cell_ = NULL;
    end_ = true;
    is_del_ = false;
    return;
  }

  // Allocate space for the cell
  cell_size_ = array_schema->cell_size(attribute_ids_);
  var_size_ = (cell_size_ == VAR_SIZE);
  if(!var_size_)
    cell_ = malloc(cell_size_);
  else 
    cell_ = NULL;

  // Get first cell
  init_iterators();
  int fragment_id = get_next_cell(); 
  if(fragment_id != -1)
    advance_cell(fragment_id); 
}

template<class T>
ArrayConstDenseCellIterator<T>::ArrayConstDenseCellIterator(
    Array* array, const T* range) : array_(array) {
  // Initialize private attributes
  const ArraySchema* array_schema = array_->array_schema();
  attribute_num_ =array_schema->attribute_num();
  dim_num_ = array_schema->dim_num();
  fragment_num_ = array_->fragment_num();
  end_ = false;
  return_del_ = false;
  range_ = new T[2*dim_num_];
  full_overlap_ = new bool[fragment_num_];
  memcpy(range_, range, 2*dim_num_*sizeof(T)); 
  cell_buffer_size_ = CELL_BUFFER_INITIAL_SIZE;
  cell_its_ = NULL;
  coords_match_ = false;
  current_coords_ = new T[dim_num_];
  array_schema->get_domain_start<T>(current_coords_, range_); 
  tile_its_ = NULL;
  array_schema->init_zero_cell(zero_cell_);

  // Prepare the ids of the fragments the iterator will iterate on
  // By default it is all the fragments, but this may change in the future
  for(int i=0; i<fragment_num_; ++i)
    fragment_ids_.push_back(i);

  // Prepare the ids of the attributes the iterator will iterate on
  for(int i=0; i<=attribute_num_; ++i)
    attribute_ids_.push_back(i);

  // Case where the array is empty
  if(array_->empty()) {
    cell_size_ = 0;
    var_size_ = false;
    cell_ = NULL;
    end_ = true;
    is_del_ = false;
    return;
  }

  // Allocate space for the cell
  cell_size_ = array_schema->cell_size(attribute_ids_);
  var_size_ = (cell_size_ == VAR_SIZE);
  if(!var_size_)
    cell_ = malloc(cell_size_);
  else 
    cell_ = NULL;

  // Get first cell
  init_iterators_in_range();
  for(int i=0; i<fragment_ids_.size(); ++i)
    find_next_cell_in_range(fragment_ids_[i]);
  int fragment_id = get_next_cell(); 
  if(fragment_id != -1)
    advance_cell_in_range(fragment_id); 
}

template<class T>
ArrayConstDenseCellIterator<T>::ArrayConstDenseCellIterator(
    Array* array, 
    const T* range,
    const std::vector<int>& attribute_ids) : array_(array) {
  // Check fragment and attribute ids
  assert(array_->array_schema()->valid_attribute_ids(attribute_ids));

  // Initialize private attributes
  const ArraySchema* array_schema = array_->array_schema();
  attribute_num_ = array_schema->attribute_num();
  dim_num_ = array_schema->dim_num();
  fragment_num_ = array_->fragment_num();
  end_ = false;
  return_del_ = false;
  range_ = new T[2*dim_num_];
  full_overlap_ = new bool[fragment_num_];
  memcpy(range_, range, 2*dim_num_*sizeof(T)); 
  cell_buffer_size_ = CELL_BUFFER_INITIAL_SIZE;
  cell_its_ = NULL;
  coords_match_ = false;
  current_coords_ = new T[dim_num_];
  array_schema->get_domain_start<T>(current_coords_, range_); 
  tile_its_ = NULL;
  array_schema->init_zero_cell(zero_cell_);

  // Prepare the ids of the fragments the iterator will iterate on
  // By default it is all the fragments, but this may change in the future
  for(int i=0; i<fragment_num_; ++i)
    fragment_ids_.push_back(i);

  // Prepare the ids of the attributes the iterator will iterate on
  // The coordinates must always be part of the attributes to be
  // iterated on (to allow visitng cell of multiple fragments in order)
  attribute_ids_ = attribute_ids;
  // In case the attribute_ids is empty, include a dummy attribute to
  // gracefully handle deletions.
  if(attribute_ids_.size() == 0) 
    attribute_ids_.push_back(array_schema->smallest_attribute());
  attribute_ids_.push_back(attribute_num_);
  attribute_ids_ = rdedup(attribute_ids_);

  // Case where the array is empty
  if(array_->empty()) {
    cell_size_ = 0;
    var_size_ = false;
    cell_ = NULL;
    end_ = true;
    is_del_ = false;
    return;
  }

  // Allocate space for the cell
  cell_size_ = array_schema->cell_size(attribute_ids_);
  var_size_ = (cell_size_ == VAR_SIZE);
  if(!var_size_)
    cell_ = malloc(cell_size_);
  else 
    cell_ = NULL;

  // Get first cell
  init_iterators_in_range();
  for(int i=0; i<fragment_num_; ++i)
    find_next_cell_in_range(fragment_ids_[i]);
  int fragment_id = get_next_cell(); 
  if(fragment_id != -1)
    advance_cell_in_range(fragment_id); 
}

template<class T>
ArrayConstDenseCellIterator<T>::~ArrayConstDenseCellIterator() {
  if(cell_ != NULL) 
    free(cell_);

  if(cell_its_ != NULL) {
    for(int i=0; i<fragment_num_; ++i)
      delete [] cell_its_[i];
    delete [] cell_its_; 
  }

  if(tile_its_ != NULL) {
    for(int i=0; i<fragment_num_; ++i) {
      delete [] tile_its_[i];
    }
    delete [] tile_its_; 
  }

  if(range_ != NULL)
    delete [] range_;

  if(current_coords_ != NULL)
    delete [] current_coords_;

  if(full_overlap_ != NULL)
    delete [] full_overlap_;

  if(zero_cell_ != NULL)
    free(zero_cell_);
}

/******************************************************
********************* ACCESSORS ***********************
******************************************************/

template<class T>
const ArraySchema* ArrayConstDenseCellIterator<T>::array_schema() const {
  return array_->array_schema();
}


template<class T>
const std::vector<int>& ArrayConstDenseCellIterator<T>::attribute_ids() const {
  return attribute_ids_;
}

template<class T>
size_t ArrayConstDenseCellIterator<T>::cell_size() const {
  assert(!end_);

  if(!var_size_) {
    return cell_size_;
  } else {
    size_t cell_size;
    size_t coords_size = array_->array_schema()->cell_size(attribute_num_);
     memcpy(&cell_size, static_cast<char*>(cell_) + coords_size, 
            sizeof(size_t));
    return cell_size;
  }
}

template<class T>
size_t ArrayConstDenseCellIterator<T>::cell_size(
    int fragment_id) const {

  if(!var_size_) {
    return cell_size_;
  } else {
    size_t cell_size = sizeof(size_t); 
    for(int i=0; i<attribute_ids_.size(); ++i)
      cell_size += cell_its_[fragment_id][attribute_ids_[i]].cell_size();
    return cell_size; 
  }
}

template<class T>
bool ArrayConstDenseCellIterator<T>::end() const {
  return end_;
}

/******************************************************
********************* OPERATORS ***********************
******************************************************/

template<class T>
void ArrayConstDenseCellIterator<T>::operator++() {
  if(coords_match_) {
    int fragment_id = get_next_cell();

    // Advance cell
    if(fragment_id != -1) {
      if(range_ != NULL) 
        advance_cell_in_range(fragment_id);
      else 
        advance_cell(fragment_id);
    } 
  }

  // For easy reference
  const T* cell_coords = static_cast<const T*>(cell_); 

  // Advance dense cell coordinates
  bool within_domain = 
      array_->array_schema()->advance_coords<T>(
          current_coords_, static_cast<const T*>(range_));

  // Clean up if coordinates exceeded the domain
  if(!within_domain) {
    delete [] current_coords_;
    current_coords_ = NULL;
    end_ = true;
  }
}

template<class T>
const void* ArrayConstDenseCellIterator<T>::operator*() {
  while(is_del_ && !return_del_ && cell_ != NULL)
    ++(*this);

  // No more dense cells
  if(current_coords_ == NULL)
    return NULL;

  // For easy reference
  const T* cell_coords = static_cast<const T*>(cell_);
  const ArraySchema* array_schema = array_->array_schema();

  if(cell_ == NULL || 
     !array_schema->coords_match<T>(current_coords_, cell_coords)) {
    coords_match_ = false;
    // Update the zero cell with the current coordinates
    memcpy(zero_cell_, current_coords_, array_schema->coords_size());
    return zero_cell_; 
  } else {
    coords_match_ = true;
    return cell_;
  } 
}

/******************************************************
****************** PRIVATE METHODS ********************
******************************************************/

template<class T>
void ArrayConstDenseCellIterator<T>::advance_cell(
    int fragment_id) {
  // Advance cell iterators
  for(int j=0; j<attribute_ids_.size(); ++j)
    ++cell_its_[fragment_id][attribute_ids_[j]];

  // Potentially advance also tile iterators
  if(cell_its_[fragment_id][attribute_num_].end()) {
    // Advance tile iterators
    for(int j=0; j<attribute_ids_.size(); ++j) 
      ++tile_its_[fragment_id][attribute_ids_[j]];

    // Initialize cell iterators
    if(!tile_its_[fragment_id][attribute_num_].end()) {
      for(int j=0; j<attribute_ids_.size(); ++j) 
        cell_its_[fragment_id][attribute_ids_[j]] = 
            (*tile_its_[fragment_id][attribute_ids_[j]])->begin();
    }
  }
}

template<class T>
void ArrayConstDenseCellIterator<T>::advance_cell_in_range(
    int fragment_id) {
  // Advance cell iterators
  for(int j=0; j<attribute_ids_.size(); ++j)
    ++cell_its_[fragment_id][attribute_ids_[j]];

  find_next_cell_in_range(fragment_id);
}


template<class T>
void ArrayConstDenseCellIterator<T>::find_next_cell_in_range(
    int fragment_id) {
  // The loop will be broken when a cell in range is found, or
  // all cells are exhausted
  while(1) { 
    // If not in the end of the tile
    if(!cell_its_[fragment_id][attribute_num_].end() &&
       !full_overlap_[fragment_id]) {
      const void* coords;
      const T* point;
      while(!cell_its_[fragment_id][attribute_num_].end()) {
        coords = *cell_its_[fragment_id][attribute_num_];
        point = static_cast<const T*>(coords);
        if(inside_range(point, range_, dim_num_)) 
          break; // cell found
        ++cell_its_[fragment_id][attribute_num_];
      }
    }

    // If the end of the tile is reached (cell not found yet)
    if(cell_its_[fragment_id][attribute_num_].end()) {
      // Advance coordinate tile iterator
      ++tile_its_[fragment_id][attribute_num_];

      // Find the first coordinate tile that overlaps with the range
      const T* mbr;
      std::pair<bool, bool> tile_overlap;
      while(!tile_its_[fragment_id][attribute_num_].end()) {
        mbr = static_cast<const T*>(
                  tile_its_[fragment_id][attribute_num_].mbr());
        tile_overlap = overlap(mbr, range_, dim_num_); 
        if(tile_overlap.first) { 
          full_overlap_[fragment_id] = tile_overlap.second;
          break;  // next tile found
        }
        ++tile_its_[fragment_id][attribute_num_];
      } 

      if(tile_its_[fragment_id][attribute_num_].end())
        break; // cell cannot be found
      else // initialize coordinates cell iterator
        cell_its_[fragment_id][attribute_num_] = 
            (*tile_its_[fragment_id][attribute_num_])->begin();

    } else { // Not the end of the cells in this tile
      break; // cell found
    }
  }

  // Synchronize attribute cell and tile iterators
  for(int j=0; j<attribute_ids_.size()-1; ++j) {
    tile_its_[fragment_id][attribute_ids_[j]] = 
      array_->begin(fragment_id, attribute_ids_[j]);
    tile_its_[fragment_id][attribute_ids_[j]] +=  
        tile_its_[fragment_id][attribute_num_].pos();
    if(!tile_its_[fragment_id][attribute_ids_[j]].end()) {
      cell_its_[fragment_id][attribute_ids_[j]] = 
          (*tile_its_[fragment_id][attribute_ids_[j]])->begin();
      cell_its_[fragment_id][attribute_ids_[j]] +=  
          cell_its_[fragment_id][attribute_num_].pos();
    } else {
      cell_its_[fragment_id][attribute_ids_[j]] = Tile::end(); 
    }
  }    
}

template<class T>
int ArrayConstDenseCellIterator<T>::get_next_cell() {
  // For easy reference
  size_t coords_size = array_->array_schema()->cell_size(attribute_num_);

  // Get the first non-NULL coordinates
  const void *coords, *next_coords;
  int fragment_id;
  int f = 0;
  do {
    next_coords = *cell_its_[fragment_ids_[f]][attribute_num_]; 
    ++f;
  } while((next_coords == NULL) && (f < fragment_ids_.size())); 

  fragment_id = fragment_ids_[f-1];

  // Get the next coordinates in the global cell order
  for(int i=f; i<fragment_ids_.size(); ++i) {
    coords = *cell_its_[fragment_ids_[i]][attribute_num_]; 
    if(coords != NULL) {
      if(memcmp(coords, next_coords, coords_size) == 0) {
        if(range_ != NULL)
          advance_cell_in_range(fragment_id);
        else
          advance_cell(fragment_id); 
        fragment_id = fragment_ids_[i];
      } else if(precedes(cell_its_[fragment_ids_[i]][attribute_num_],
                         cell_its_[fragment_id][attribute_num_])) {
        next_coords = coords;
        fragment_id = fragment_ids_[i];
      }     
    }
  }

  if(next_coords != NULL) { // There are cells
    // --- Prepare cell ---
    // Find cell size and create a new cell for variable-sized cells
    if(var_size_) {
      cell_size_ = this->cell_size(fragment_id);
      size_t new_cell_buffer_size = cell_buffer_size_;
      while(new_cell_buffer_size < cell_size_)
        new_cell_buffer_size *= 2;
      if(cell_ == NULL) {
        cell_ = malloc(new_cell_buffer_size);
      } else if(new_cell_buffer_size > cell_buffer_size_) {
        free(cell_);
        cell_ = malloc(new_cell_buffer_size);
      }  
      cell_buffer_size_ = new_cell_buffer_size;
    } 
    char* cell = static_cast<char*>(cell_);
    size_t offset;

    // Copy coordinates to cell
    memcpy(cell, *(cell_its_[fragment_id][attribute_num_]), coords_size);
    offset = coords_size;
    // Copy cell size for variable-sized cells
    if(var_size_) {
      memcpy(cell + offset, &cell_size_, sizeof(size_t));
      offset += sizeof(size_t);
    }

    // Copy attributes to cell
    size_t attr_size;
    for(int j=0; j<attribute_ids_.size()-1; ++j) { 
      attr_size = cell_its_[fragment_id][attribute_ids_[j]].cell_size();
      memcpy(cell + offset, *(cell_its_[fragment_id][attribute_ids_[j]]), 
             attr_size);
      offset += attr_size;
    }

    // Check if the retrieved cell represents a deletion
    assert(attribute_ids_[0] != attribute_num_);
    is_del_ = cell_its_[fragment_id][attribute_ids_[0]].is_del();

    return fragment_id;
  } else { // No more cells
    if(cell_ != NULL) 
      free(cell_);
    cell_ = NULL;
    is_del_ = false;
    return -1;
  }
}

template<class T>
void ArrayConstDenseCellIterator<T>::init_iterators() {
  // Create tile and cell iterators
  tile_its_ = new FragmentConstTileIterator*[fragment_num_];
  cell_its_ = new TileConstCellIterator*[fragment_num_];

  for(int i=0; i<fragment_num_; ++i) {
   tile_its_[i] = new FragmentConstTileIterator[attribute_num_+1];
   cell_its_[i] = new TileConstCellIterator[attribute_num_+1];
  }

  // Initialize iterators
  for(int i=0; i<fragment_ids_.size(); ++i) {
    for(int j=0; j<attribute_ids_.size(); ++j) {
      tile_its_[fragment_ids_[i]][attribute_ids_[j]] = 
          array_->begin(fragment_ids_[i], attribute_ids_[j]);
      cell_its_[fragment_ids_[i]][attribute_ids_[j]] = 
          (*tile_its_[fragment_ids_[i]][attribute_ids_[j]])->begin();
    }
  }
}

template<class T>
void ArrayConstDenseCellIterator<T>::init_iterators_in_range() {
  // Create tile and cell iterators
  tile_its_ = new FragmentConstTileIterator*[fragment_num_];
  cell_its_ = new TileConstCellIterator*[fragment_num_];

  for(int i=0; i<fragment_num_; ++i) {
    tile_its_[i] = new FragmentConstTileIterator[attribute_num_+1];
    cell_its_[i] = new TileConstCellIterator[attribute_num_+1];
  }

  // Initialize tile and cell iterators 
  for(int i=0; i<fragment_ids_.size(); ++i) { 
    // Initialize coordinate tile iterator
    tile_its_[fragment_ids_[i]][attribute_num_] =  
        array_->begin(fragment_ids_[i], attribute_num_);

    // Find the first coordinate tile that overlaps with the range
    const T* mbr;
    std::pair<bool, bool> tile_overlap;
    while(!tile_its_[fragment_ids_[i]][attribute_num_].end()) {
      mbr = static_cast<const T*>
                (tile_its_[fragment_ids_[i]][attribute_num_].mbr());
      tile_overlap = overlap(mbr, range_, dim_num_); 

      if(tile_overlap.first) { 
        full_overlap_[fragment_ids_[i]] = tile_overlap.second;
        break;
      }
      ++tile_its_[fragment_ids_[i]][attribute_num_];
    } 

    // Syncronize attribute tile iterators
    for(int j=0; j<attribute_ids_.size()-1; ++j) {
      tile_its_[fragment_ids_[i]][attribute_ids_[j]] = 
          array_->begin(fragment_ids_[i], attribute_ids_[j]);
      tile_its_[fragment_ids_[i]][attribute_ids_[j]] += 
          tile_its_[fragment_ids_[i]][attribute_num_].pos();
    }    

    // Initialize cell iterators
    for(int j=0; j<attribute_ids_.size(); ++j) {
      if(!tile_its_[fragment_ids_[i]][attribute_ids_[j]].end())
        cell_its_[fragment_ids_[i]][attribute_ids_[j]] = 
            (*tile_its_[fragment_ids_[i]][attribute_ids_[j]])->begin();
      else
        cell_its_[fragment_ids_[i]][attribute_ids_[j]] = Tile::end();
    }
  }
}

template<class T>
bool ArrayConstDenseCellIterator<T>::precedes(
    const TileConstCellIterator& it_A,
    const TileConstCellIterator& it_B) const {
  const void* coords_A = *it_A;
  const void* coords_B = *it_B;
  int64_t tile_id_A = it_A.tile_id();
  int64_t tile_id_B = it_B.tile_id();
  bool regular = array_->array_schema()->has_regular_tiles();

  // Case #1 for true: regular + it_A has smaller tile id
  if(regular && tile_id_A < tile_id_B)
    return true;

  bool coords_A_precede = 
      array_->array_schema()->precedes(static_cast<const T*>(coords_A), 
                                       static_cast<const T*>(coords_B));

  // Case #2 for true: regular + equal tile ids + coords of it_A precede
  if(regular && tile_id_A == tile_id_B && coords_A_precede)
    return true;

  // Case #3 for true: iregular + coords of it_A precede
  if(!regular && coords_A_precede)
    return true;

  // False in all other cases
  return false;
}

// Explicit template instantiations
template class ArrayConstDenseCellIterator<int>;
template class ArrayConstDenseCellIterator<int64_t>;
template class ArrayConstDenseCellIterator<float>;
template class ArrayConstDenseCellIterator<double>;