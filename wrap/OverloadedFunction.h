/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file OverloadedFunction.h
 * @brief Function that can overload its arguments only
 * @author Frank Dellaert
 * @date Nov 13, 2014
 **/

#pragma once

#include "Function.h"
#include "Argument.h"
#include <unordered_set>
namespace wrap {

/**
 * ArgumentList Overloads
 */
class ArgumentOverloads {
public:
  std::vector<ArgumentList> argLists_;

public:
  size_t nrOverloads() const { return argLists_.size(); }

  const ArgumentList& argumentList(size_t i) const { return argLists_.at(i); }

  void push_back(const ArgumentList& args) { argLists_.push_back(args); }

  std::vector<ArgumentList> expandArgumentListsTemplate(
      const TemplateSubstitution& ts) const {
    std::vector<ArgumentList> result;
    for (const ArgumentList& argList : argLists_) {
      ArgumentList instArgList = argList.expandTemplate(ts);
      result.push_back(instArgList);
    }
    return result;
  }

  /// Expand templates, imperative !
  virtual void ExpandTemplate(const TemplateSubstitution& ts) {
    argLists_ = expandArgumentListsTemplate(ts);
  }

  void verifyArguments(const std::vector<std::string>& validArgs,
                       const std::string s) const {
    for (const ArgumentList& argList : argLists_) {
      for (Argument arg : argList) {
        std::string fullType = arg.type.qualifiedName("::");
        if (find(validArgs.begin(), validArgs.end(), fullType) ==
            validArgs.end())
          throw DependencyMissing(fullType, "checking argument of " + s);
      }
    }
  }

  friend std::ostream& operator<<(std::ostream& os,
                                  const ArgumentOverloads& overloads) {
    for (const ArgumentList& argList : overloads.argLists_)
      os << argList << std::endl;
    return os;
  }

  std::string pyx_resolveOverloadParams(const ArgumentList& args, bool isVoid, size_t indentLevel = 2) const {
    std::string indent;
    for (size_t i = 0; i<indentLevel; ++i) indent += "\t";
    std::string s;
    s += indent + "if len(args)+len(kwargs) !=" + std::to_string(args.size()) + ":\n";
    s += indent + "\treturn False";
    s += (!isVoid) ? ", None\n" : "\n";
    if (args.size() > 0) {
      s += indent + "__params = kwargs.copy()\n";
      s += indent + "__names = [" + args.pyx_paramsList() + "]\n";
      s += indent + "for i in range(len(args)):\n"; 
      s += indent + "\t__params[__names[i]] = args[i]\n";
      s += indent + "try:\n";
      s += args.pyx_castParamsToPythonType();
      s += indent + "except:\n";
      s += indent + "\treturn False";
      s += (!isVoid) ? ", None\n" : "\n";
    }
    return s;
  }

  /// if two overloading methods have the same number of arguments, they have
  /// to be resolved via keyword args
  std::string pyx_checkDuplicateNargsKwArgs(size_t indentLevel = 2) const {
    std::string indent;
    for (size_t i = 0; i<indentLevel; ++i) indent += "\t";
    std::unordered_set<size_t> nargsSet;
    std::vector<size_t> nargsDuplicates;
    for (size_t i = 0; i < nrOverloads(); ++i) {
      size_t nargs = argumentList(i).size();
      if (nargsSet.find(nargs) != nargsSet.end())
        nargsDuplicates.push_back(nargs);
      else
        nargsSet.insert(nargs);
    }

    std::string s;
    if (nargsDuplicates.size() > 0) {
      s += indent + "if len(kwargs)==0 and len(args)+len(kwargs) in [";
      for (size_t i = 0; i < nargsDuplicates.size(); ++i) {
        s += std::to_string(nargsDuplicates[i]);
        if (i < nargsDuplicates.size() - 1) s += ",";
      }
      s += "]:\n";
      s += indent + "\traise TypeError('Overloads with the same number of " 
           "arguments exist. Please use keyword arguments to " 
           "differentiate them!')\n";
    }
    return s;
  }
};

class OverloadedFunction : public Function, public ArgumentOverloads {
public:
  bool addOverload(const std::string& name, const ArgumentList& args,
                   boost::optional<const Qualified> instName = boost::none,
                   bool verbose = false) {
    bool first = initializeOrCheck(name, instName, verbose);
    ArgumentOverloads::push_back(args);
    return first;
  }

private:
};

// Templated checking functions
// TODO: do this via polymorphism, use transform ?

template <class F>
static std::map<std::string, F> expandMethodTemplate(
    const std::map<std::string, F>& methods, const TemplateSubstitution& ts) {
  std::map<std::string, F> result;
  typedef std::pair<const std::string, F> NamedMethod;
  for (NamedMethod namedMethod : methods) {
    F instMethod = namedMethod.second;
    instMethod.expandTemplate(ts);
    namedMethod.second = instMethod;
    result.insert(namedMethod);
  }
  return result;
}

template <class F>
inline void verifyArguments(const std::vector<std::string>& validArgs,
                            const std::map<std::string, F>& vt) {
  typedef typename std::map<std::string, F>::value_type NamedMethod;
  for (const NamedMethod& namedMethod : vt)
    namedMethod.second.verifyArguments(validArgs);
}

}  // \namespace wrap