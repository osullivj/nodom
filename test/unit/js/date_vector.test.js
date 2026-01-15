// Why are we using vitest not jest?
// https://stackoverflow.com/questions/50793885/referenceerror-you-are-trying-to-import-a-file-after-the-jest-environment-has
// https://jestjs.io/docs/ecmascript-modules
// https://github.com/cmcculloh/jest-import
// And that is exactly why we need to get away from React/Angular JS GUIs
import { describe, expect, test } from "vitest";
import {
  DateDay,
  TimestampSecond,
  DateMillisecond,
  TimestampMillisecond,
  TimestampMicrosecond,
  TimestampNanosecond,
  RecordBatchReader,
  Table,
  vectorFromArray,
} from "apache-arrow";

describe("DateVector", () => {
  describe(`TimestampVector`, () => {
    test(`Dates are stored in TimestampMillisecond`, () => {
      const date = new Date("2023-02-01T12:34:56Z");
      const vec = vectorFromArray([date]);
      expect(vec.type).toBeInstanceOf(TimestampMillisecond);
      expect(vec.get(0)).toBe(date.getTime());
    });

    test(`Correctly get back TimestampSecond from Date`, () => {
      const date = new Date("2023-02-01T12:34:56Z");
      const vec = vectorFromArray([date], new TimestampSecond());
      expect(vec.type).toBeInstanceOf(TimestampSecond);
      expect(vec.get(0)).toBe(date.getTime());
    });

    test(`Correctly get back TimestampMicrosecond from Date`, () => {
      const date = new Date("2023-02-01T12:34:56Z");
      const vec = vectorFromArray([date, 0.5], new TimestampMicrosecond());
      expect(vec.type).toBeInstanceOf(TimestampMicrosecond);
      expect(vec.get(0)).toBe(date.getTime());
      expect(vec.get(1)).toBe(0.5);
    });

    test(`Correctly get back TimestampNanosecond from Date`, () => {
      const date = new Date("2023-02-01T12:34:56Z");
      const vec = vectorFromArray([date, 0.5], new TimestampNanosecond());
      expect(vec.type).toBeInstanceOf(TimestampNanosecond);
      expect(vec.get(0)).toBe(date.getTime());
      expect(vec.get(1)).toBe(0.5);
    });
  });

  describe(`DateVector`, () => {
    test(`returns days since the epoch as correct JS Dates`, () => {
      const table = new Table(RecordBatchReader.from(test_data));
      const expectedMillis = expectedMillis32();
      const date32 = table.getChildAt(0);
      for (const date of date32) {
        const millis = expectedMillis.shift();
        expect(date).toEqual(millis);
      }
    });

    test(`returns millisecond longs since the epoch as correct JS Dates`, () => {
      const table = new Table(RecordBatchReader.from(test_data));
      const expectedMillis = expectedMillis64();
      const date64 = table.getChildAt(1);
      for (const date of date64) {
        const millis = expectedMillis.shift();
        expect(date).toEqual(millis);
      }
    });

    test(`returns the same date that was in the vector`, () => {
      const dates = [new Date(1950, 1, 0)];
      const vec = vectorFromArray(dates, new DateMillisecond());
      for (const date of vec) {
        expect(date).toEqual(dates.shift()?.getTime());
      }
    });
  });

  const expectedMillis32 = () => [
    165247430400000,
    34582809600000,
    232604524800000,
    null,
    199808812800000,
    165646771200000,
    209557238400000,
    null,
  ];

  const expectedMillis64 = () => [
    27990830234011,
    -41278585914325,
    12694624797111,
    null,
    null,
    10761360520213,
    null,
    1394015437000,
  ];

  const test_data = {
    schema: {
      fields: [
        {
          name: "f0",
          type: {
            name: "date",
            unit: "DAY",
          },
          nullable: true,
          children: [],
        },
        {
          name: "f1",
          type: {
            name: "date",
            unit: "MILLISECOND",
          },
          nullable: true,
          children: [],
        },
      ],
    },
    batches: [
      {
        count: 8,
        columns: [
          {
            name: "f0",
            count: 8,
            VALIDITY: [1, 1, 1, 0, 1, 1, 1, 0],
            DATA: [
              1912586, 400264, 2692182, 2163746, 2312602, 1917208, 2425431,
            ],
          },
          {
            name: "f1",
            count: 8,
            VALIDITY: [1, 1, 1, 0, 0, 1, 0, 1],
            DATA: [
              27990830234011,
              -41278585914325,
              12694624797111,
              -38604948562547,
              -37802308043516,
              10761360520213,
              -25129181633384,
              1394015437000, // <-- the tricky one
            ],
          },
        ],
      },
    ],
  };
});
