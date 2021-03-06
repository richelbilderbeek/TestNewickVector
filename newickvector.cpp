//---------------------------------------------------------------------------
/*
NewickVector, class to store a Newick as a std::vector<int>
Copyright (C) 2010-2015 Richel Bilderbeek

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program.If not, see <http://www.gnu.org/licenses/>.
*/
//---------------------------------------------------------------------------
//From http://www.richelbilderbeek.nl/CppNewickVector.htm
//---------------------------------------------------------------------------
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include "newickvector.h"

#include <algorithm>
#include <cassert>
#include <deque>
#include <iostream>
#include <functional>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

#include <boost/numeric/conversion/cast.hpp>

#include <boost/lexical_cast.hpp>

#include "BigIntegerLibrary.hh"

#include "fuzzy_equal_to.h"
#include "newick.h"

#pragma GCC diagnostic pop

ribi::NewickVector::NewickVector(const std::string& s)
  : m_v{newick::StringToNewick(s)}
{
  assert(newick::IsNewick(s));
  //Can this be added?
  //assert(m_v.empty() || newick::IsNewick(m_v));
}

ribi::NewickVector::NewickVector(const std::vector<int>& v)
  : m_v{v}
{
  assert(m_v.empty() || newick::IsNewick(m_v));
}

BigInteger ribi::NewickVector::CalcComplexity() const
{
  return newick::CalcComplexity(m_v);
}

double ribi::NewickVector::CalcDenominator(const double theta) const
{
  return newick::CalcDenominator(Peek(),theta);
}

BigInteger ribi::NewickVector::CalcNumOfCombinations() const
{
  assert(newick::IsNewick(m_v));
  return newick::CalcNumOfCombinationsBinary(m_v);
}

BigInteger ribi::NewickVector::CalcNumOfSymmetries() const
{
  assert(newick::IsNewick(m_v));
  assert(newick::IsBinaryNewick(m_v));
  return newick::CalcNumOfSymmetriesBinary(m_v);
}

double ribi::CalculateProbabilityNewickVector(
  const std::string& newick_str,
  const double theta)
{
  assert(newick::IsNewick(newick_str));
  assert(theta > 0.0);
  NewickVector newick(newick_str);
  NewickStorage<NewickVector> storage(newick);
  return CalculateProbabilityImpl(
    newick,
    theta,
    storage
  );
}

double ribi::CalculateProbabilityImpl(
  const NewickVector& n,
  const double theta,
  NewickStorage<NewickVector>& storage)
{
  while(1)
  {
    //Is n already known?
    {
      const double p = storage.Find(n);
      if (p!=0.0)
      {
        return p;
      }
    }

    //Check for simple phylogeny
    if (n.IsSimple())
    {
      const double p = n.CalcProbabilitySimpleNewick(theta);
      storage.Store(n,p);
      return p;
    }
    //Complex
    //Generate other Newicks and their coefficients
    const auto cnp = GetCoefficientNewickPairs(n, theta);
    const std::vector<double>& coefficients = cnp.first;
    const std::vector<NewickVector>& newicks = cnp.second;
    //Ask help about these new Newicks
    {
      const int sz = newicks.size();
      assert(newicks.size() == coefficients.size() );
      double p = 0.0;
      for (int i=0; i!=sz; ++i)
      {
        //Recursive function call
        p+=(coefficients[i] * CalculateProbabilityImpl(newicks[i],theta,storage));
      }
      storage.Store(n,p);
      return p;
    }
  }
}

double ribi::NewickVector::CalcProbabilitySimpleNewick(const double theta) const
{
  assert(newick::IsSimple(m_v));
  assert(theta > 0.0);
  return newick::CalcProbabilitySimpleNewick(m_v,theta);
}

int ribi::NewickVector::FindPosAfter(const std::vector<int>& v,const int x, const int index) const
{
  const int sz = v.size();
  for (int i=index; i!=sz; ++i)
  {
    assert(i >= 0);
    assert(i < sz);
    if (v[i]==x) return i;
  }
  return sz;
}

int ribi::NewickVector::FindPosBefore(const std::vector<int>& v,const int x, const int index) const
{

  for (int i=index; i!=-1; --i)
  {
    #ifndef NDEBUG
    const int sz = static_cast<int>(v.size());
    assert(i >= 0);
    assert(i < sz);
    #endif
    if (v[i]==x) return i;
  }
  return -1;
}


std::pair<std::vector<double>, std::vector<ribi::NewickVector>>
ribi::GetCoefficientNewickPairs(
  const NewickVector& n,
  const double theta
)
{
  std::vector<double> coefficients;
  std::vector<NewickVector> newicks;
  const double d = n.CalcDenominator(theta);
  typedef std::pair<std::vector<int>,int> NewickFrequencyPair;
  const std::vector<NewickFrequencyPair> newick_freqs
    = newick::GetSimplerNewicksFrequencyPairs(n.Peek());
  for(const NewickFrequencyPair& p: newick_freqs)
  {
    const int frequency = p.second;
    assert(frequency > 0);
    if (frequency == 1)
    {
      newicks.push_back(p.first);
      coefficients.push_back(theta / d);
    }
    else
    {
      const double f_d = static_cast<double>(frequency);
      newicks.push_back(p.first);
      coefficients.push_back( (f_d*(f_d-1.0)) / d);
    }
  }
  return std::make_pair(coefficients, newicks);
}

std::vector<ribi::NewickVector> ribi::NewickVector::GetSimplerNewicks() const
{
  assert(newick::IsNewick(m_v));
  const std::vector<std::vector<int> > v
    = newick::GetSimplerBinaryNewicks(m_v);
  std::vector<NewickVector> w(std::begin(v),std::end(v));
  return w;
}

std::pair<ribi::NewickVector,ribi::NewickVector> ribi::NewickVector::GetRootBranches() const
{
  assert(newick::IsNewick(m_v));
  std::pair<std::vector<int>,std::vector<int> > p
    = newick::GetRootBranchesBinary(m_v);
  return p;
}

std::string ribi::GetNewickVectorVersion() noexcept
{
  return "2.1";
}

std::vector<std::string> ribi::GetNewickVectorVersionHistory() noexcept
{
  return {
    "2009-06-01: Version 1.0: Initial version",
    "2010-08-10: Version 1.1: Major architectural revision",
    "2011-02-20: Version 1.2: Removed helper functions from global namespace",
    "2011-02-22: Version 2.0: Changed file management",
    "2011-04-08: Version 2.1: fixed error forgiven by G++, but fatal for i686-pc-mingw32-qmake"
  };
}

 bool ribi::NewickVector::IsCloseBracketRight(const int pos) const
{
  const int sz = m_v.size();

  assert(pos >= 0);
  assert(pos < sz);
  assert(m_v[pos]==1);

  for (int i=pos+1; i!=sz; ++i) //+1 because v[pos]==1
  {
    const int x = m_v[i];
    if (x == newick::bracket_close) return true;
    if (x == newick::bracket_open) return false;
  }
  //There will always be a final closing bracket at the right
  // that is not stored in a SortedNewickVector's std::vector
  return true;
}

bool ribi::NewickVector::IsOpenBracketLeft(const int pos) const
{
  assert(pos >= 0);
  assert(pos < static_cast<int>(m_v.size()));
  assert(m_v[pos]==1);

  for (int i=pos-1; i!=-1; --i) //-1, because v[pos]==1
  {
    const int x = m_v[i];
    if (x == newick::bracket_open) return true;
    if (x == newick::bracket_close) return false;
  }
  //There will always be a trailing opening bracket at the left
  // that is not stored in a SortedNewickVector's std::vector
  return true;
}

bool ribi::NewickVector::IsSimple() const
{
  return newick::IsSimple(m_v);
}

//Does the following conversions:
// (5,(5,1)) -> (5,6)
// (4,(5,1)) -> (4,6)
// (4,(3,1)) -> (4,4)
// (4,(1,1)) -> (4,2)
// string_pos points at an index in the string current.newick after the '1'
// For example, for (4,(3,1)) the string_pos equals 7
// num is the other value between brackets
// For example, for (4,(3,1)) num will equal 3
// (5,(5,1)) -> (5,6)
// -> sz = 9
// -> bracket_open_pos  = 3
// -> bracket_close_pos = 7
// -> sz_loss = 4 = 7 - 3 = bracket_close_pos - bracket_open_pos
// -> new_sz = 5
ribi::NewickVector ribi::NewickVector::LoseBrackets(const int x, const int i) const
{
  assert(i >= 0);
  assert(i < Size());
  assert(m_v[i] == 1);
  assert(x>0);
  std::vector<int> v_copy = m_v;

  const int bracket_open_pos
    = FindPosBefore(m_v,newick::bracket_open,i);
  assert(bracket_open_pos > -1);
  const int bracket_close_pos
    = FindPosAfter(m_v,newick::bracket_close,i);
  assert(bracket_close_pos < Size());
  const int sz = Size();
  const int sz_lose = bracket_close_pos - bracket_open_pos;
  const int sz_new = sz - sz_lose;
  v_copy[bracket_open_pos] = x+1;
  const std::vector<int>::iterator begin_iter(&v_copy[bracket_close_pos+1]);
  const std::vector<int>::iterator output_iter(&v_copy[bracket_open_pos+1]);
  std::copy(begin_iter,v_copy.end(),output_iter);
  v_copy.resize(sz_new);

  return NewickVector(v_copy);
}

bool ribi::NewickVectorCompare(
  const std::vector<int>& lhs,
  const std::vector<int>& rhs
) noexcept
{
  const int l_sz = lhs.size();
  const int r_sz = rhs.size();
  if (l_sz < r_sz) return true;
  if (l_sz > r_sz) return false;

  typedef std::vector<int>::const_iterator Iter;
  Iter lhs_iter = lhs.begin();
  const Iter lhs_end = lhs.end();
  Iter rhs_iter = rhs.begin();

  for ( ; lhs_iter != lhs_end; ++lhs_iter, ++rhs_iter)
  {
    const int x_l = *lhs_iter;
    const int x_r = *rhs_iter;
    if (x_l < x_r) return true;
    if (x_l > x_r) return false;
  }
  return false;
}

int ribi::NewickVector::Size() const noexcept
{
  return boost::numeric_cast<int>(m_v.size());
}

ribi::NewickVector ribi::NewickVector::TermIsNotOne(const int i) const
{
  assert(m_v[i]>1);
  std::vector<int> v(m_v);
  --v[i];
  return NewickVector(v);
}

//TermIsOne is called whenever a '1' is found in a newick structure
//string_pos has the index of the character after this '1'
// (when a string has multiple 1's, TermIsOne is called for each '1',
//  with each time a different string_pos)
//If this '1' is between two brackets, with one other number,
//  these two numbers are added and the brackets are removed
//If this '1' is not between two brackets,
//  the newick string returned is empty
//Conversion examples
// (3,(15,1)), string_pos 8 -> (3,16)
//         ^   EXIT1
// (2,(23,1)), string_pos 8 -> (2,24)
//         ^   EXIT1
// (1,(20,5)), string_pos 2 -> [empty]
//   ^         EXIT-2
// (1,(1,1)), string_pos 2 -> [empty]
//   ^         EXIT-2
// (1,(1,1)), string_pos 5 -> (1,2)
//      ^      EXIT-2
// (1,(1,1)), string_pos 7 -> (1,2)
//        ^    EXIT-1
// ((1,2,3),3), string_pos 3 -> (3,3)   [*1]
//    ^
//
// Notes:
// [*1]: Might be incorrect: algorithm holds for two numbers between brackets
ribi::NewickVector ribi::NewickVector::TermIsOne(const int i) const
{
  assert(i >= 0);
  assert(i < static_cast<int>(m_v.size()));
  assert(!m_v.empty());
  assert(m_v[i] == 1); //Must be a 1

  if (!IsOpenBracketLeft(i) || !IsCloseBracketRight(i))
  {
    //Return an empty SortedNewickVector
    return NewickVector(std::vector<int>());
  }
  //Find other_value
  const int other_value{
    TermIsOneFindOtherValue(i)
  };
  assert(other_value >= 1);
  return LoseBrackets(other_value,i);
}

int ribi::NewickVector::TermIsOneFindOtherValue(const int i) const
{
  const int sz{static_cast<int>(m_v.size())};
  assert(i < sz);
  assert(!m_v.empty());
  assert(m_v[i] == 1); //Must be a 1
  int other_value = 0;
  //If adjecent to the left is a comma
  // and subsequently a value,
  if (i > 0  && m_v[i-1]  > 0)
  {
    other_value = m_v[i-1];
  }
  else if (i + 1 < sz && m_v[i+1] > 0)
  {
    other_value = m_v[i+1];
  }
  return other_value;
}

std::string ribi::NewickVector::ToStr() const
{
  assert(newick::IsNewick(m_v));
  return newick::NewickToString(m_v);
}

bool ribi::operator<(const NewickVector& lhs, const NewickVector& rhs) noexcept
{
  return ribi::NewickVectorCompare(lhs.Peek(),rhs.Peek());
}
