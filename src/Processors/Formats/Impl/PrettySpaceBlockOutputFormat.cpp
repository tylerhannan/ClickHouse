#include <Common/PODArray.h>
#include <IO/WriteBuffer.h>
#include <IO/WriteHelpers.h>
#include <Formats/FormatFactory.h>
#include <Processors/Formats/Impl/PrettySpaceBlockOutputFormat.h>


namespace DB
{


void PrettySpaceBlockOutputFormat::write(Chunk chunk, PortKind port_kind)
{
    UInt64 max_rows = format_settings.pretty.max_rows;

    if (total_rows >= max_rows)
    {
        total_rows += chunk.getNumRows();
        return;
    }

    size_t num_rows = chunk.getNumRows();
    size_t num_columns = chunk.getNumColumns();
    const auto & header = getPort(port_kind).getHeader();
    const auto & columns = chunk.getColumns();

    WidthsPerColumn widths;
    Widths max_widths;
    Widths name_widths;
    calculateWidths(header, chunk, widths, max_widths, name_widths);

    if (format_settings.pretty.output_format_pretty_row_numbers)
    {
        writeString(String(row_number_width, ' '), out);
    }
    /// Names
    for (size_t i = 0; i < num_columns; ++i)
    {
        if (i != 0)
            writeCString("   ", out);
        else
            writeChar(' ', out);

        const ColumnWithTypeAndName & col = header.getByPosition(i);

        if (col.type->shouldAlignRightInPrettyFormats())
        {
            for (ssize_t k = 0; k < std::max(static_cast<ssize_t>(0), static_cast<ssize_t>(max_widths[i] - name_widths[i])); ++k)
                writeChar(' ', out);

            if (format_settings.pretty.color)
                writeCString("\033[1m", out);
            writeString(col.name, out);
            if (format_settings.pretty.color)
                writeCString("\033[0m", out);
        }
        else
        {
            if (format_settings.pretty.color)
                writeCString("\033[1m", out);
            writeString(col.name, out);
            if (format_settings.pretty.color)
                writeCString("\033[0m", out);

            for (ssize_t k = 0; k < std::max(static_cast<ssize_t>(0), static_cast<ssize_t>(max_widths[i] - name_widths[i])); ++k)
                writeChar(' ', out);
        }
    }
    writeCString("\n\n", out);

    for (size_t row = 0; row < num_rows && total_rows + row < max_rows; ++row)
    {
        if (format_settings.pretty.output_format_pretty_row_numbers)
        {
            // Write row number;
            auto row_num_string = std::to_string(row + 1) + ". ";
            for (size_t i = 0; i < row_number_width - row_num_string.size(); ++i)
            {
                writeCString(" ", out);
            }
            writeString(row_num_string, out);
        }
        for (size_t column = 0; column < num_columns; ++column)
        {
            if (column != 0)
                writeCString(" ", out);

            const auto & type = *header.getByPosition(column).type;
            auto & cur_width = widths[column].empty() ? max_widths[column] : widths[column][row];
            writeValueWithPadding(*columns[column], *serializations[column],
                row, cur_width, max_widths[column], type.shouldAlignRightInPrettyFormats());
        }

        writeChar('\n', out);
    }

    total_rows += num_rows;
}


void PrettySpaceBlockOutputFormat::writeSuffix()
{
    if (total_rows >= format_settings.pretty.max_rows)
    {
        writeCString("\nShowed first ", out);
        writeIntText(format_settings.pretty.max_rows, out);
        writeCString(".\n", out);
    }
}


void registerOutputFormatPrettySpace(FormatFactory & factory)
{
    factory.registerOutputFormat("PrettySpace", [](
        WriteBuffer & buf,
        const Block & sample,
        const RowOutputFormatParams &,
        const FormatSettings & format_settings)
    {
        return std::make_shared<PrettySpaceBlockOutputFormat>(buf, sample, format_settings);
    });

    factory.markOutputFormatSupportsParallelFormatting("PrettySpace");

    factory.registerOutputFormat("PrettySpaceNoEscapes", [](
        WriteBuffer & buf,
        const Block & sample,
        const RowOutputFormatParams &,
        const FormatSettings & format_settings)
    {
        FormatSettings changed_settings = format_settings;
        changed_settings.pretty.color = false;
        return std::make_shared<PrettySpaceBlockOutputFormat>(buf, sample, changed_settings);
    });

    factory.markOutputFormatSupportsParallelFormatting("PrettySpaceNoEscapes");
}

}
