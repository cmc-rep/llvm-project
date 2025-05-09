//===-- CommandReturnObject.cpp -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "lldb/Interpreter/CommandReturnObject.h"

#include "lldb/Utility/DiagnosticsRendering.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/StreamString.h"

using namespace lldb;
using namespace lldb_private;

static llvm::raw_ostream &error(Stream &strm) {
  return llvm::WithColor(strm.AsRawOstream(), llvm::HighlightColor::Error,
                         llvm::ColorMode::Enable)
         << "error: ";
}

static llvm::raw_ostream &warning(Stream &strm) {
  return llvm::WithColor(strm.AsRawOstream(), llvm::HighlightColor::Warning,
                         llvm::ColorMode::Enable)
         << "warning: ";
}

static llvm::raw_ostream &note(Stream &strm) {
  return llvm::WithColor(strm.AsRawOstream(), llvm::HighlightColor::Note,
                         llvm::ColorMode::Enable)
         << "note: ";
}

static void DumpStringToStreamWithNewline(Stream &strm, const std::string &s) {
  bool add_newline = false;
  if (!s.empty()) {
    // We already checked for empty above, now make sure there is a newline in
    // the error, and if there isn't one, add one.
    strm.Write(s.c_str(), s.size());

    const char last_char = *s.rbegin();
    add_newline = last_char != '\n' && last_char != '\r';
  }
  if (add_newline)
    strm.EOL();
}

CommandReturnObject::CommandReturnObject(bool colors)
    : m_out_stream(colors), m_err_stream(colors), m_colors(colors) {}

void CommandReturnObject::AppendErrorWithFormat(const char *format, ...) {
  SetStatus(eReturnStatusFailed);

  if (!format)
    return;
  va_list args;
  va_start(args, format);
  StreamString sstrm;
  sstrm.PrintfVarArg(format, args);
  va_end(args);

  const std::string &s = std::string(sstrm.GetString());
  if (!s.empty()) {
    error(GetErrorStream());
    DumpStringToStreamWithNewline(GetErrorStream(), s);
  }
}

void CommandReturnObject::AppendMessageWithFormat(const char *format, ...) {
  if (!format)
    return;
  va_list args;
  va_start(args, format);
  StreamString sstrm;
  sstrm.PrintfVarArg(format, args);
  va_end(args);

  GetOutputStream() << sstrm.GetString();
}

void CommandReturnObject::AppendNoteWithFormat(const char *format, ...) {
  if (!format)
    return;
  va_list args;
  va_start(args, format);
  StreamString sstrm;
  sstrm.PrintfVarArg(format, args);
  va_end(args);

  note(GetOutputStream()) << sstrm.GetString();
}

void CommandReturnObject::AppendWarningWithFormat(const char *format, ...) {
  if (!format)
    return;
  va_list args;
  va_start(args, format);
  StreamString sstrm;
  sstrm.PrintfVarArg(format, args);
  va_end(args);

  warning(GetErrorStream()) << sstrm.GetString();
}

void CommandReturnObject::AppendMessage(llvm::StringRef in_string) {
  if (in_string.empty())
    return;
  GetOutputStream() << in_string.rtrim() << '\n';
}

void CommandReturnObject::AppendNote(llvm::StringRef in_string) {
  if (in_string.empty())
    return;
  note(GetOutputStream()) << in_string.rtrim() << '\n';
}

void CommandReturnObject::AppendWarning(llvm::StringRef in_string) {
  if (in_string.empty())
    return;
  warning(GetErrorStream()) << in_string.rtrim() << '\n';
}

void CommandReturnObject::AppendError(llvm::StringRef in_string) {
  SetStatus(eReturnStatusFailed);
  if (in_string.empty())
    return;
  // Workaround to deal with already fully formatted compiler diagnostics.
  llvm::StringRef msg(in_string.rtrim());
  msg.consume_front("error: ");
  error(GetErrorStream()) << msg << '\n';
}

void CommandReturnObject::SetError(Status error) {
  SetError(error.takeError());
}

void CommandReturnObject::SetError(llvm::Error error) {
  // Retrieve any diagnostics.
  error = llvm::handleErrors(std::move(error), [&](DiagnosticError &error) {
    SetStatus(eReturnStatusFailed);
    m_diagnostics = error.GetDetails();
  });
  if (error) {
    AppendError(llvm::toString(std::move(error)));
  }
}

std::string
CommandReturnObject::GetInlineDiagnosticString(unsigned indent) const {
  StreamString diag_stream(m_colors);
  RenderDiagnosticDetails(diag_stream, indent, true, m_diagnostics);
  // Duplex the diagnostics to the secondary stream (but not inlined).
  if (auto stream_sp = m_err_stream.GetStreamAtIndex(eImmediateStreamIndex))
    RenderDiagnosticDetails(*stream_sp, std::nullopt, false, m_diagnostics);

  return diag_stream.GetString().str();
}

std::string CommandReturnObject::GetErrorString(bool with_diagnostics) const {
  StreamString stream(m_colors);
  if (with_diagnostics)
    RenderDiagnosticDetails(stream, std::nullopt, false, m_diagnostics);

  lldb::StreamSP stream_sp(m_err_stream.GetStreamAtIndex(eStreamStringIndex));
  if (stream_sp)
    stream << std::static_pointer_cast<StreamString>(stream_sp)->GetString();
  return stream.GetString().str();
}

StructuredData::ObjectSP CommandReturnObject::GetErrorData() {
  return Serialize(m_diagnostics);
}

// Similar to AppendError, but do not prepend 'Status: ' to message, and don't
// append "\n" to the end of it.

void CommandReturnObject::AppendRawError(llvm::StringRef in_string) {
  SetStatus(eReturnStatusFailed);
  assert(!in_string.empty() && "Expected a non-empty error message");
  GetErrorStream() << in_string;
}

void CommandReturnObject::SetStatus(ReturnStatus status) { m_status = status; }

ReturnStatus CommandReturnObject::GetStatus() const { return m_status; }

bool CommandReturnObject::Succeeded() const {
  return m_status <= eReturnStatusSuccessContinuingResult;
}

bool CommandReturnObject::HasResult() const {
  return (m_status == eReturnStatusSuccessFinishResult ||
          m_status == eReturnStatusSuccessContinuingResult);
}

void CommandReturnObject::Clear() {
  lldb::StreamSP stream_sp;
  stream_sp = m_out_stream.GetStreamAtIndex(eStreamStringIndex);
  if (stream_sp)
    static_cast<StreamString *>(stream_sp.get())->Clear();
  stream_sp = m_err_stream.GetStreamAtIndex(eStreamStringIndex);
  if (stream_sp)
    static_cast<StreamString *>(stream_sp.get())->Clear();
  m_diagnostics.clear();
  m_status = eReturnStatusStarted;
  m_did_change_process_state = false;
  m_suppress_immediate_output = false;
  m_interactive = true;
}

bool CommandReturnObject::GetDidChangeProcessState() const {
  return m_did_change_process_state;
}

void CommandReturnObject::SetDidChangeProcessState(bool b) {
  m_did_change_process_state = b;
}

bool CommandReturnObject::GetInteractive() const { return m_interactive; }

void CommandReturnObject::SetInteractive(bool b) { m_interactive = b; }

bool CommandReturnObject::GetSuppressImmediateOutput() const {
  return m_suppress_immediate_output;
}

void CommandReturnObject::SetSuppressImmediateOutput(bool b) {
  m_suppress_immediate_output = b;
}
