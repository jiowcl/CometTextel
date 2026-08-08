// CometTextel.NET - CometTextel API for .NET
// Copyright (c) 2026 Jiowcl. All rights reserved.

namespace CometTextel.NET.Core
{
    /// <summary>
    /// Exception thrown when a CometTextel native call fails.
    /// </summary>
    public sealed class CometTextelException : Exception
    {
        public CometTextelException(int status, string message)
            : base(message)
        {
            Status = status;
        }

        public int Status { get; }
    }
}
