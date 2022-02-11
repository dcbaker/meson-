// SPDX-license-identifier: Apache-2.0
// Copyright © 2021 Intel Corporation

/**
 * Meson++ Mid level IR
 *
 * This IR is lossy, it doesn't contain all of the information that the AST
 * does, and is designed for running lower passes on, so we can get it closer to
 * the backend IR, removing all function calls and most varibles.
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <variant>
#include <vector>

#include "toolchains/toolchain.hpp"

namespace fs = std::filesystem;

namespace MIR {

/**
 * Information about an object when it is stored to a variable
 *
 * At the MIR level, assignments are stored to the object, as many
 * objects have creation side effects (creating a Target, for example)
 *
 * The name will be referenced against the symbol table, along with the version
 * which is used by value numbering.
 */
class Variable {
  public:
    Variable();
    Variable(const std::string & n);
    Variable(const std::string & n, const uint32_t & v);
    Variable(const Variable & v);

    std::string name;

    /// The version as used by value numbering, 0 means unset
    uint32_t version;

    explicit operator bool() const;
    bool operator<(const Variable &) const;
    bool operator==(const Variable &) const;
};

/**
 * Holds a File, which is a smart object point to a source
 *
 */
class File {
  public:
    File(const std::string & name_, const fs::path & sdir, const bool & built_,
         const fs::path & sr_, const fs::path & br_);
    File(const std::string & name_, const fs::path & sdir, const bool & built_,
         const fs::path & sr_, const fs::path & br_, const Variable & v);

    /// Whether this is a built object, or a static one
    const bool is_built() const;

    /// Get the name of the of the file, relative to the src dir if it's static,
    /// or the build dir if it's built
    std::string get_name() const;

    /// Get a path for this file relative to the source tree
    fs::path relative_to_source_dir() const;

    /// Get a path for this file relative to the build treeZ
    fs::path relative_to_build_dir() const;

    bool operator==(const File &) const;
    bool operator!=(const File &) const;

    // For gtest
    friend std::ostream & operator<<(std::ostream & os, const File & f);

    const std::string name;
    const fs::path subdir;
    const bool built;
    const fs::path source_root;
    const fs::path build_root;

    Variable var;
};

class CustomTarget;

/// Input sources for most targets
using Source = std::variant<std::shared_ptr<File>, std::shared_ptr<CustomTarget>>;

class CustomTarget {
  public:
    CustomTarget(const std::string & n, const std::vector<Source> & i, const std::vector<File> & o,
                 const std::vector<std::string> & c, const fs::path & s, const Variable & v);

    const std::string name;
    const std::vector<Source> inputs;
    const std::vector<File> outputs;
    const std::vector<std::string> command;
    const fs::path subdir;

    Variable var;
};

using ArgMap = std::unordered_map<Toolchain::Language, std::vector<Arguments::Argument>>;

enum class StaticLinkMode {
    NORMAL,
    WHOLE,
};

class StaticLibrary;

using StaticLinkage = std::tuple<StaticLinkMode, const StaticLibrary *>;
using Source = std::variant<std::shared_ptr<File>, std::shared_ptr<CustomTarget>>;

class Executable {
  public:
    Executable(const std::string & name_, const std::vector<Source> & srcs,
               const Machines::Machine & m, const fs::path & sdir, const ArgMap & args,
               const std::vector<StaticLinkage>& s_link, const Variable & v);

    /// The name of the target
    const std::string name;

    /// The sources (as files)
    const std::vector<Source> sources;

    /// Which machine is this executable to be built for?
    const Machines::Machine machine;

    /// Where is this Target defined
    const fs::path subdir;

    /**
     * Arguments for the target, sorted by langauge
     *
     * We sort these by language, as each compiled source will only recieve it's
     * per-language arguments
     */
    const ArgMap arguments;

    /// static targets to link with
    const std::vector<StaticLinkage> link_static{};

    Variable var;

    std::string output() const;
};

class StaticLibrary {
  public:
    StaticLibrary(const std::string & name_, const std::vector<Source> & srcs,
                  const Machines::Machine & m, const fs::path & sdir, const ArgMap & args,
                  const std::vector<StaticLinkage>& s_link, const Variable & v);

    /// The name of the target
    const std::string name;

    /// The sources (as files)
    const std::vector<Source> sources;

    /// Which machine is this executable to be built for?
    const Machines::Machine machine;

    /// Where is this Target defined
    const fs::path subdir;

    /**
     * Arguments for the target, sorted by langauge
     *
     * We sort these by language, as each compiled source will only recieve it's
     * per-language arguments
     */
    const ArgMap arguments;

    /// static targets to link with
    const std::vector<StaticLinkage> link_static{};

    Variable var;

    std::string output() const;
};

/**
 * A phi node
 *
 * A synthetic instruction which represents the point where to possible values
 * for a variable converge. When one strictly dominates the other then this can
 * be removed.
 */
class Phi {
  public:
    Phi();
    Phi(const uint32_t & l, const uint32_t & r, const Variable & v);

    uint32_t left;
    uint32_t right;

    bool operator==(const Phi & other) const;
    bool operator<(const Phi & other) const;

    Variable var;
};

class IncludeDirectories {
  public:
    IncludeDirectories(const std::vector<std::string> & d, const bool & s, const Variable & v);

    const std::vector<std::string> directories;
    const bool is_system;

    Variable var;
};

enum class DependencyType {
    INTERNAL,
};

/**
 * A dependency object
 *
 * Holds files, arguments, etc, to apply to build targets
 */
class Dependency {
  public:
    Dependency(const std::string & name, const bool & found, const std::string & version,
               const std::vector<Arguments::Argument> & args, const Variable & var);

    /// Name of the dependency
    const std::string name;

    /// whether or not the dependency is found
    const bool found;

    /// The version of the dependency
    const std::string version;

    /// Per-language compiler args
    const std::vector<Arguments::Argument> arguments;

    /// The kind of dependency this is
    const DependencyType type = DependencyType::INTERNAL;

    Variable var;
};

enum class MessageLevel {
    DEBUG,
    MESSAGE,
    WARN,
    ERROR,
};

class Message {
  public:
    Message(const MessageLevel & l, const std::string & m);

    /// What level or kind of message this is
    const MessageLevel level;

    /// The message itself
    const std::string message;

    Variable var;
};

class Program {
  public:
    Program(const std::string & n, const Machines::Machine & m, const fs::path & p);
    Program(const std::string & n, const Machines::Machine & m, const fs::path & p,
            const Variable & v);

    const std::string name;
    const Machines::Machine for_machine;
    const fs::path path;

    bool found() const;

    Variable var;
};

class Empty {
  public:
    Empty(){};

    Variable var;
};

/*
 * Thse objects "Wrap" a lower level object, and provide interfaces for user
 * defined data. Their main job is to take the user data, validate it, and call
 * into the lower level objects
 */
class Array;
class Boolean;
class Dict;
class FunctionCall;
class Identifier;
class Number;
class String;
class Compiler;

using Object =
    std::variant<std::shared_ptr<FunctionCall>, std::shared_ptr<String>, std::shared_ptr<Boolean>,
                 std::shared_ptr<Number>, std::unique_ptr<Identifier>, std::shared_ptr<Array>,
                 std::shared_ptr<Dict>, std::shared_ptr<Compiler>, std::shared_ptr<File>,
                 std::shared_ptr<Executable>, std::shared_ptr<StaticLibrary>, std::unique_ptr<Phi>,
                 std::shared_ptr<IncludeDirectories>, std::unique_ptr<Message>,
                 std::shared_ptr<Program>, std::unique_ptr<Empty>, std::shared_ptr<CustomTarget>,
                 std::shared_ptr<Dependency>>;

/**
 * Holds a toolchain
 *
 * Called compiler as that's what it is in the Meson DSL
 */
class Compiler {
  public:
    Compiler(const std::shared_ptr<MIR::Toolchain::Toolchain> & tc);

    const std::shared_ptr<MIR::Toolchain::Toolchain> toolchain;

    const Object get_id(const std::vector<Object> &,
                        const std::unordered_map<std::string, Object> &) const;

    Variable var;
};

// Can be a method via an optional paramter maybe?
/// A function call object
class FunctionCall {
  public:
    FunctionCall(const std::string & _name, std::vector<Object> && _pos,
                 std::unordered_map<std::string, Object> && _kw, const std::filesystem::path & _sd);
    FunctionCall(const std::string & _name, std::vector<Object> && _pos,
                 const std::filesystem::path & _sd);

    const std::string name;

    /// Ordered container of positional argument objects
    std::vector<Object> pos_args;

    /// Unordered container mapping keyword arguments to their values
    std::unordered_map<std::string, Object> kw_args;

    /// reference to object holding this function, if it's a method.
    std::optional<Object> holder;

    /**
     * The directory this was called form.
     *
     * For functions that care (such as file(), and most targets()) this is
     * required to accurately map sources between the source and build dirs.
     */
    const std::filesystem::path source_dir;

    Variable var;
};

class String {
  public:
    String(const std::string & f);

    bool operator==(const String &) const;
    bool operator!=(const String &) const;

    const std::string value;
    Variable var;
};

class Boolean {
  public:
    Boolean(const bool & f);
    Boolean(const bool & f, const Variable & v);

    bool operator==(const Boolean &) const;
    bool operator!=(const Boolean &) const;

    const bool value;
    Variable var;
};

class Number {
  public:
    Number(const int64_t & f);

    bool operator==(const Number &) const;
    bool operator!=(const Number &) const;

    const int64_t value;
    Variable var;
};

class Identifier {
  public:
    Identifier(const std::string & s);
    Identifier(const std::string & s, const uint32_t & ver, Variable && v);

    /// The name of the identifier
    const std::string value;

    /**
     * The Value numbering version
     *
     * This is only relavent in a couple of situations, namely when we've
     * replaced a phi with an identifer, and we need to be clear which version
     * this is an alias of:
     *
     *      x₄ = x₁
     *      x₅ = ϕ(x₃, x₄)
     *
     * In this case we need to know that x₄ is x₁, and not any other version.
     * however, x₄ should be promptly cleaned up by a constant folding pass,
     * removing the need to track this information long term.
     */
    uint32_t version;

    Variable var;
};

class Array {
  public:
    Array();
    Array(std::vector<Object> && a);

    std::vector<Object> value;
    Variable var;
};

class Dict {
  public:
    Dict();

    // TODO: the key is allowed to be a string or an expression that evaluates
    // to a string, we need to enforce that somewhere.
    std::unordered_map<std::string, Object> value;
    Variable var;
};

class BasicBlock;

/**
 * A think that creates A conditional web.
 *
 * This works such that `if_true` will always point to a Basic block, and
 * `if_false` will either point to andother Condition or nothing. This means
 * that our web will always have a form like:
 *
 *    O --\
 *  /      \
 * O   O --\\
 *  \ /     \\
 *   O   O - O
 *    \ /   /
 *     O   /
 *      \ /
 *       O
 *
 * Because the false condition will itself be a condition.
 *
 * if_false is initialized to nullptr, and one needs to check for that.
 */
class Condition {
  public:
    Condition(Object && o);
    Condition(Object && o, std::shared_ptr<BasicBlock> s);

    /// An object that is the condition
    Object condition;

    /// The block to go to if the condition is true
    std::shared_ptr<BasicBlock> if_true;

    /// The block to go to if the condition is false
    std::shared_ptr<BasicBlock> if_false;
};

using NextType =
    std::variant<std::monostate, std::unique_ptr<Condition>, std::shared_ptr<BasicBlock>>;

class BasicBlock;

struct BBComparitor {
    bool operator()(const BasicBlock * lhs, const BasicBlock * rhs) const;
};

/**
 * Holds a list of instructions, and optionally a condition or next block
 */
class BasicBlock {
  public:
    BasicBlock();
    BasicBlock(std::unique_ptr<Condition> &&);

    /// The instructions in this block
    std::list<Object> instructions;

    /// Either nothing, a pointer to another BasicBlock, or a pointer to a Condition
    NextType next;

    /// All potential parents of this block
    std::set<BasicBlock *, BBComparitor> parents;

    const uint32_t index;

    bool operator<(const BasicBlock &) const;
};

} // namespace MIR
