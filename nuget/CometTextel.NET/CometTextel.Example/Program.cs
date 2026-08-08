// CometTextel.NET - CometTextel API for .NET
// Copyright (c) 2026 Jiowcl. All rights reserved.

using CometTextel.NET.Core;

if (args.Length == 0)
{
    PrintHelp();
    return 1;
}

try
{
    return args[0].ToLowerInvariant() switch
    {
        "pdu" => RunPdu(args),
        "send" => RunSend(args),
        "list" => RunList(args),
        "delete" => RunDelete(args),
        _ => FailHelp($"Unknown command: {args[0]}")
    };
}
catch (CometTextelException ex)
{
    Console.Error.WriteLine($"CometTextel error ({ex.Status}): {ex.Message}");
    return 2;
}

/// <summary>
/// Run the PDU command.
/// </summary>
/// <param name="args">The arguments.</param>
/// <returns>The status code.</returns>
static int RunPdu(
    string[] args)
{
    // pdu <destination> <text> [smsc]
    if (args.Length < 3)
    {
        return FailHelp("Usage: pdu <destination> <text> [smsc]");
    }

    var smsc = args.Length >= 4 ? args[3] : string.Empty;
    var hex = Pdu.EncodeSubmit(args[1], args[2], smsc, DataCoding.Ucs2);

    Console.WriteLine(hex);

    var decoded = Pdu.Decode(hex);
    Console.WriteLine($"peer={decoded.PeerAddress} text={decoded.UserData} coding={decoded.Coding}");

    return 0;
}

/// <summary>
/// Run the Send command.
/// </summary>
/// <param name="args">The arguments.</param>
/// <returns>The status code.</returns>
static int RunSend(
    string[] args)
{
    // send <port> <smsc> <destination> <text>
    if (args.Length < 5)
    {
        return FailHelp("Usage: send <port> <smsc> <destination> <text>");
    }

    using var modem = new GsmModem();

    modem.Open(args[1]);
    modem.Send(args[3], args[4], args[2], DataCoding.Ucs2);
    Console.WriteLine("Sent.");

    return 0;
}

/// <summary>
/// Run the List command.
/// </summary>
/// <param name="args">The arguments.</param>
/// <returns>The status code.</returns>
static int RunList(
    string[] args)
{
    // list <port>
    if (args.Length < 2)
    {
        return FailHelp("Usage: list <port>");
    }

    using var modem = new GsmModem();
    modem.Open(args[1]);

    var messages = modem.List();

    Console.WriteLine($"Found {messages.Count} message(s)");

    foreach (var message in messages)
    {
        Console.WriteLine(
            $"index={message.Index} from={message.PeerAddress} udh={message.HasUdh} text={message.UserData}");
    }

    return 0;
}

/// <summary>
/// Run the Delete command.
/// </summary>
/// <param name="args">The arguments.</param>
/// <returns>The status code.</returns>
static int RunDelete(
    string[] args)
{
    // delete <port> <index>
    if (args.Length < 3 || !int.TryParse(args[2], out var index))
    {
        return FailHelp("Usage: delete <port> <index>");
    }

    using var modem = new GsmModem();

    modem.Open(args[1]);
    modem.Delete(index);
    Console.WriteLine($"Deleted index {index}");

    return 0;
}

/// <summary>
/// Print the help message.
/// </summary>
static void PrintHelp()
{
    Console.WriteLine("""
        CometTextel.Example commands:
          pdu <destination> <text> [smsc]
          send <port> <smsc> <destination> <text>
          list <port>
          delete <port> <index>
        """);
}

/// <summary>
/// Fail with help message.
/// </summary>
/// <param name="message">The message.</param>
/// <returns>The status code.</returns>
static int FailHelp(string message)
{
    Console.Error.WriteLine(message);
    PrintHelp();

    return 1;
}
