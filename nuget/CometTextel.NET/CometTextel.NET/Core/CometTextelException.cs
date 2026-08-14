// CometTextel.NET - CometTextel API for .NET
// Copyright (c) 2026 Jiowcl. All rights reserved.

namespace CometTextel.NET.Core
{
    /// <summary>
    /// Exception thrown when a CometTextel native call fails.
    /// </summary>
    public sealed class CometTextelException : Exception
    {
        /// <summary>
        /// Creates a new CometTextelException.
        /// </summary>
        /// <param name="status">The status code.</param>
        /// <param name="message">The message.</param>
        public CometTextelException(
            int status, 
            string message)
            : base(message)
        {
            Status = status;
        }

        /// <summary>
        /// The status code.
        /// </summary>
        public int Status { get; }
    }
}
