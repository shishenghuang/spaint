/**
 * rafl: RandomTreeChopper.h
 * Copyright (c) Torr Vision Group, University of Oxford, 2015. All rights reserved.
 */

#ifndef H_RAFL_RANDOMTREECHOPPER
#define H_RAFL_RANDOMTREECHOPPER

#include "TreeChopper.h"

namespace rafl {

/**
 * \brief An instance of this class represents a tree chopper that chops trees in the forest at random.
 */
template <typename Label>
class RandomTreeChopper : public TreeChopper<Label>
{
  //#################### USINGS #################### 
private:
  using typename TreeChopper<Label>::RF_CPtr;

  //#################### PRIVATE VARIABLES ####################
private:
  /** The random number generator to use when deciding which tree to chop. */
  mutable tvgutil::RandomNumberGenerator m_rng;

  //#################### CONSTRUCTORS ####################
public:
  /**
   * \brief Constructs a random tree chopper.
   *
   * \param seed  The seed for the random number generator.
   */
  explicit RandomTreeChopper(unsigned int seed)
  : m_rng(seed)
  {}

  //#################### PUBLIC MEMBER FUNCTIONS ####################
public:
  /** Override */
  virtual boost::optional<size_t> choose_tree_to_chop(const RF_CPtr& forest) const
  {
    return m_rng.generate_int_from_uniform(0, static_cast<int>(forest->get_tree_count()) - 1);
  }
};

}

#endif
